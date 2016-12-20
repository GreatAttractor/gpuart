/*
GPU-Assisted Ray Tracer
Copyright (C) 2016 Filip Szczerek <ga.software@yahoo.com>

This file is part of gpuart.

Gpuart is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Gpuart is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with gpuart.  If not, see <http://www.gnu.org/licenses/>.

File description:
    Main program file
*/

#include <algorithm>
#include <cmath>
#include <chrono>
#include <initializer_list>
#include <random>
#include <fstream>
#include <iostream>
#include <memory>
#include <nanogui/nanogui.h>
#include <sstream>
#include <stdexcept>
#include <string>

#include "core.h"
#include "gl_utils.h"
#include "math_types.h"
#include "renderer.h"
#include "scenes.h"
#include "utils.h"


#define PI 3.1415926

using gpuart::Vec3f;
using gpuart::Vec3d;

// --------------------------------------------------------

class GPUARTApp: public nanogui::Screen
{

    double TPrevSec;

    struct
    {
        const float MAX_FOVY = 120; // degrees
        const float MIN_FOVY = 5;  // degrees

        const float MAX_SPEED = 12; // zoom factor (in or out) applied per second when zooming
        const float ACCEL = 40;
        const float DELAY_S = 0.15f; // time interval of zoom acceleration after the last zoom event

        const float SPEED_NONE = 1.0f;

        float speed = SPEED_NONE; // current rate of FOVY change (times per second), signed
        bool zoomingIn;
        double tLastZoomEvent, tLastZoomUpdate;
    } Zoom;

    struct
    {
        enum class Mode { DirectLighting, PathTracing };

        Mode mode = Mode::DirectLighting;

        const unsigned MAX_PATHS_PER_PIXEL = 1024;

        unsigned PathsPerPixel = MAX_PATHS_PER_PIXEL;
        unsigned PathsPerPass = 1;
    } Rendering;

    struct
    {
        enum class MouseMode { Camera, UserSphere };

        // "Dragging active" for left, right, middle mouse button
        struct
        {
            bool lmb, rmb, mmb;
        } Dragging = { false, false, false };

        // Position of the last "button down" event
        struct
        {
            Eigen::Vector2i lmb, rmb, mmb;
        } LastDown;

        MouseMode Mode = MouseMode::Camera;
    } Controls;

    struct
    {
        gpuart::Camera Cam;

        const float LIN_MV_SCALE = 10;  // Linear scale of camera movement
        const float ANG_MV_SCALE = 2; // Angular scale of camera movement (later scaled by current FOV)

        Vec3f DragOrigin;
        struct { Vec3f dir, up; } RotOrigin;

        /// 'true' if user input has caused camera change and the initial rays have to be re-initialized
        bool MustUpdate = false;
    } Camera;

    /// User-controlled sphere in the scene
    struct
    {
        const float LIN_MV_SCALE = 10; // Linear scale movement
        Vec3f DragOrigin;
    } UserSphere;

    std::unique_ptr<gpuart::Renderer> Renderer;

    struct
    {
        std::vector<nanogui::Window*> Windows;

        const double PATH_COUNT_CHANGE_DELAY_SEC = 0.5;
        double tLastPathsChange;
        bool pathsChanged = false;

        struct
        {
            nanogui::Label *renderSpeed;
            nanogui::Label *screenSize;
            struct
            {
                nanogui::ProgressBar *pbar;
                nanogui::Label *label;

                nanogui::Widget *w;
            } PathTracingProgress;

        } Info;
    } GUI;

    void UpdatePathTracingProgress(unsigned pathsRendered, unsigned totalPaths)
    {
        GUI.Info.PathTracingProgress.label->setCaption(gpuart::Utils::FormatStr("Paths/pixel: %u/%u", pathsRendered, totalPaths).get());
        GUI.Info.PathTracingProgress.pbar->setValue((float)pathsRendered/totalPaths);
    }

    void UpdateScreenSizeInfo(int w, int h)
    {
        GUI.Info.screenSize->setCaption(gpuart::Utils::FormatStr("%dx%d (%.1f Mpix)", w, h, w*h/1000000.0).get());
    }

    void SetVertLayout(nanogui::Window *wnd)
    {
        wnd->setLayout(new nanogui::BoxLayout(nanogui::Orientation::Vertical, nanogui::Alignment::Minimum, 10, 5));
    }

    nanogui::Widget *CreateHorzBox(nanogui::Widget &parent)
    {
        nanogui::Widget *box = new nanogui::Widget(&parent);
        box->setLayout(new nanogui::BoxLayout(nanogui::Orientation::Horizontal, nanogui::Alignment::Middle, 0, 5));
        return box;
    }

    void InitGUI()
    {
        nanogui::Widget *w;

        //----------------------------------

        nanogui::Window *wndInfo = new nanogui::Window(this, "Info");
        wndInfo->setPosition({ 10, 10 });
        SetVertLayout(wndInfo);

        GUI.Info.screenSize = new nanogui::Label(wndInfo, "", "sans-bold");

        w = CreateHorzBox(*wndInfo);
        new nanogui::Label(w, "Speed:");
        GUI.Info.renderSpeed = new nanogui::Label(w, "", "sans-bold");

        GUI.Info.PathTracingProgress.w = new nanogui::Widget(wndInfo);
        GUI.Info.PathTracingProgress.w->setLayout(new nanogui::BoxLayout(nanogui::Orientation::Vertical, nanogui::Alignment::Middle, 0, 5));
        GUI.Info.PathTracingProgress.w->setVisible(false);
        GUI.Info.PathTracingProgress.label = new nanogui::Label(GUI.Info.PathTracingProgress.w, "");
        GUI.Info.PathTracingProgress.pbar = new nanogui::ProgressBar(GUI.Info.PathTracingProgress.w);

        new nanogui::CheckBox(wndInfo, "Show windows",
                                [this](bool checked)
                                {
                                    for (auto *w: GUI.Windows)
                                        w->setVisible(checked);
                                });

        //----------------------------------

        nanogui::Window *wndLighting = new nanogui::Window(this, "Sun & Sky");
        wndLighting->setPosition({ 240, 10 });
        SetVertLayout(wndLighting);
        wndLighting->setVisible(false);
        GUI.Windows.push_back(wndLighting);

        w = CreateHorzBox(*wndLighting);
        new nanogui::Label(w, "azimuth:");

        nanogui::Slider *sunAzim = new nanogui::Slider(w);
        sunAzim->setFixedSize({ 150 / mPixelRatio, 20 / mPixelRatio });
        sunAzim->setRange(std::make_pair(0.0f, 2*3.1415926f));
        sunAzim->setCallback([this](float val) { Renderer->SetSunAzimuth(val); });
        sunAzim->setValue(Renderer->GetSunAzimuth());

        w = CreateHorzBox(*wndLighting);
        new nanogui::Label(w, "altitude:");
        nanogui::Slider *sunAlt = new nanogui::Slider(w);
        sunAlt->setFixedSize({ 150 / mPixelRatio, 20 / mPixelRatio });
        sunAlt->setRange(std::make_pair(0.1f, 3.1415926f/2));
        sunAlt->setCallback([this](float val) { Renderer->SetSunAltitude(val); });
        sunAlt->setValue(Renderer->GetSunAltitude());

        auto dirLighting = new nanogui::CheckBox(wndLighting, "Direct lighting",
                                                 [this](bool enabled) { Renderer->SetSunDirectLighting(enabled); });
        dirLighting->setChecked(Renderer->IsSunDirectLightingEnabled());

        //----------------------------------

        nanogui::Window *wndRendering = new nanogui::Window(this, "Rendering & Controls");
        wndRendering->setPosition({ 270, 40 });
        SetVertLayout(wndRendering);
        wndRendering->setVisible(false);
        GUI.Windows.push_back(wndRendering);

        nanogui::ComboBox *renderMode = new nanogui::ComboBox(wndRendering,
                                                              { "Direct lighting", "Path tracing" });
        renderMode->setCallback([this](int item)
                                {
                                    Rendering.mode = (item == 0 ? Rendering.Mode::DirectLighting
                                                                 : Rendering.Mode::PathTracing);

                                    bool ptracingEnabled = (Rendering.mode == Rendering.Mode::PathTracing);
                                    GUI.Info.PathTracingProgress.w->setVisible(ptracingEnabled);
                                    performLayout();

                                    if (ptracingEnabled)
                                    {
                                        UpdatePathTracingProgress(0, Rendering.PathsPerPixel);
                                        Renderer->RestartPathTracing(Rendering.PathsPerPass, Rendering.PathsPerPixel);
                                    }
                                });

        w = CreateHorzBox(*wndRendering);
        new nanogui::Label(w, "Paths/pass:");
        auto pathsPerPass = new nanogui::IntBox<unsigned>(w, Rendering.PathsPerPass);
        pathsPerPass->setSpinnable(true);
        pathsPerPass->setEditable(true);
        pathsPerPass->setMinMaxValues(1, Rendering.PathsPerPixel);
        pathsPerPass->setCallback([this](int val)
                                  {
                                      Rendering.PathsPerPass = val;
                                      GUI.tLastPathsChange = glfwGetTime();
                                      GUI.pathsChanged = true;
                                  });

        GUI.tLastPathsChange = glfwGetTime();

        w = CreateHorzBox(*wndRendering);
        new nanogui::Label(w, "Paths/pixel:");
        auto pathsPerPixel = new nanogui::IntBox<unsigned>(w, Rendering.PathsPerPixel);
        pathsPerPixel->setSpinnable(true);
        pathsPerPixel->setEditable(true);
        pathsPerPixel->setMinMaxValues(1, Rendering.MAX_PATHS_PER_PIXEL);
        pathsPerPixel->setCallback([=](int val)
                                   {
                                       pathsPerPass->setMaxValue(val);
                                       if ((int)pathsPerPass->value() > val)
                                           pathsPerPass->setValue(val);

                                       Rendering.PathsPerPixel = val;

                                       GUI.tLastPathsChange = glfwGetTime();
                                       GUI.pathsChanged = true;
                                   });


        w = CreateHorzBox(*wndRendering);
        w->setLayout(new nanogui::BoxLayout(nanogui::Orientation::Horizontal, nanogui::Alignment::Middle, 0, 5));
        new nanogui::Label(w, "Mouse:");
        nanogui::ComboBox *mouseMode = new nanogui::ComboBox(w, { "camera", "user sphere" });
        mouseMode->setCallback([this](int item) { Controls.Mode = (item == 0 ? Controls.MouseMode::Camera
                                                                             : Controls.MouseMode::UserSphere); });

        //----------------------------------

        nanogui::Window *wndScene = new nanogui::Window(this, "Scene");
        wndScene->setPosition({ 300, 70 });
        SetVertLayout(wndScene);
        wndScene->setVisible(false);
        GUI.Windows.push_back(wndScene);

        w = CreateHorzBox(*wndScene);
        new nanogui::Label(w, "Scene:");
        auto scenes = new nanogui::ComboBox(w, { "box",
                                                 "dragon 11k",
                                                 "dragon 48k",
                                                 "dragon 871k",
                                                 "cluster 100k",
                                                 "tree 21k" });
        scenes->setCallback([this](int sel)
            {
                switch (sel)
                {
                case 0: InitBox(*Renderer); break;
                case 1: InitDragon(*Renderer, "data/dragon_11k.ply"); break;
                case 2: InitDragon(*Renderer, "data/dragon_48k.ply"); break;
                case 3: InitDragon(*Renderer, "data/dragon_871k.ply"); break;
                case 4: InitCluster(*Renderer); break;
                case 5: InitTree(*Renderer); break;
                }

                if (Rendering.mode == Rendering.Mode::PathTracing)
                    Renderer->RestartPathTracing(Rendering.PathsPerPass, Rendering.PathsPerPixel);
            });

        w = CreateHorzBox(*wndScene);
        new nanogui::Label(w, "Sphere radius:");
        auto *sphR = new nanogui::FloatBox<float>(w, Renderer->GetUserSphereRadius());
        sphR->setMinMaxValues(0, 100.0f);
        sphR->setValueIncrement(0.1f);
        sphR->setCallback([this](float val) { Renderer->SetUserSphereRadius(val); });
        sphR->setEditable(true);
        sphR->setSpinnable(true);

        w = CreateHorzBox(*wndScene);
        new nanogui::Label(w, "Sphere:");
        auto *sphType = new nanogui::ComboBox(w, { "diffuse", "specular", "emissive" });
        sphType->setCallback([this](int item)
                             {
                                switch (item)
                                {
                                case 0:
                                    Renderer->SetUserSphereSpecular(false);
                                    Renderer->SetUserSphereEmittance(0);
                                    break;

                                case 1:
                                    Renderer->SetUserSphereSpecular(true);
                                    Renderer->SetUserSphereEmittance(0);
                                    break;

                                case 2:
                                    Renderer->SetUserSphereSpecular(false);
                                    Renderer->SetUserSphereEmittance(15);
                                    break;
                                }
                             });

        new nanogui::CheckBox(w, "fuzzy", [this](bool checked) { Renderer->SetUserSphereFuzzy(checked); });


        //----------------------------------

        setVisible(true);
        performLayout();
    }

    bool resizeEvent(const Eigen::Vector2i &size) override
    {
        Screen::resizeEvent(size);

        /** Use the framebuffer size (i.e. the one expressed in actual, physical pixels),
            not the 'size' parameter - which under some OSes on high-DPI displays may be smaller
            by some factor. We do not care for such misleading approach. */
        int newWidth = mFBSize[0],
            newHeight = mFBSize[1];

        UpdateScreenSizeInfo(newWidth, newHeight);

        glViewport(0, 0, newWidth, newHeight);
        if (!Renderer->UpdateViewportSize(newWidth, newHeight))
        {
            std::stringstream ss;
            ss << "Failed to change viewport size to " << newWidth << "x" << newHeight;
            throw std::runtime_error(ss.str());
        }

        return true;
    }

    bool scrollEvent(const Eigen::Vector2i &p, const Eigen::Vector2f &rel) override
    {
        if (Screen::scrollEvent(p, rel))
            return true;

        Zoom.zoomingIn = (rel[1] > 0);
        Zoom.tLastZoomEvent = glfwGetTime();

        return true;
    }

    bool mouseButtonEvent(const Eigen::Vector2i &p, int button, bool down, int modifiers) override
    {
        if (Screen::mouseButtonEvent(p, button, down, modifiers))
            return true;

        Eigen::Vector2i truePos = p * mPixelRatio;

        bool *dragging = nullptr;
        switch (button)
        {
        case GLFW_MOUSE_BUTTON_LEFT:   dragging = &Controls.Dragging.lmb; break;
        case GLFW_MOUSE_BUTTON_RIGHT:  dragging = &Controls.Dragging.rmb; break;
        case GLFW_MOUSE_BUTTON_MIDDLE: dragging = &Controls.Dragging.mmb; break;
        }

        if (dragging)
        {
            switch (button)
            {
            case GLFW_MOUSE_BUTTON_LEFT:
                Controls.LastDown.lmb = truePos;
                break;

            case GLFW_MOUSE_BUTTON_RIGHT:
                Controls.LastDown.rmb = truePos;
                break;

            case GLFW_MOUSE_BUTTON_MIDDLE:
                Controls.LastDown.mmb = truePos;
                break;
            }

            *dragging = down;
            if (*dragging)
            {
                switch (button)
                {
                case GLFW_MOUSE_BUTTON_LEFT:
                    Camera.RotOrigin.dir = Camera.Cam.Dir;
                    Camera.RotOrigin.up = Camera.Cam.Up;
                    break;

                case GLFW_MOUSE_BUTTON_RIGHT:
                case GLFW_MOUSE_BUTTON_MIDDLE:
                    if (Controls.Mode == Controls.MouseMode::Camera)
                        Camera.DragOrigin = Camera.Cam.Pos;
                    else
                        UserSphere.DragOrigin = Renderer->GetUserSpherePos();
                    break;
                }
            }
        }

        return true;
    }

    bool mouseMotionEvent(const Eigen::Vector2i &p, const Eigen::Vector2i &rel, int button, int modifiers) override
    {
        if (Screen::mouseMotionEvent(p, rel, button, modifiers))
            return true;

        Eigen::Vector2i truePos = p * mPixelRatio;

        gpuart::Camera &cam = Camera.Cam;

        if (Controls.Dragging.lmb)
        {
            float angleX = (truePos[0] - Controls.LastDown.lmb[0])/(float)height() * Camera.ANG_MV_SCALE * cam.FovY/100;
            cam.Dir = Vec3f::rotate(Camera.RotOrigin.dir, Camera.RotOrigin.up, sin(angleX), cos(angleX));


            float angleY = (truePos[1] - Controls.LastDown.lmb[1])/(float)height() * Camera.ANG_MV_SCALE * cam.FovY/100;
            Vec3f lateral = (cam.Dir ^ cam.Up).normalized();
            cam.Dir = Vec3f::rotate(cam.Dir, lateral, sin(angleY), cos(angleY));

            Camera.MustUpdate = true;
        }

        if (Controls.Dragging.rmb || Controls.Dragging.mmb)
        {
            Vec3f yMovement = (Controls.Dragging.rmb ? cam.Dir.normalized()
                                                      : cam.Up.normalized());

            Eigen::Vector2i &lastDown = (Controls.Dragging.rmb ? Controls.LastDown.rmb
                                                               : Controls.LastDown.mmb);

            Vec3f lateral = (cam.Dir ^ cam.Up).normalized();

            Vec3f delta = lateral   * float(lastDown[0] - truePos[0]) +
                          yMovement * float(truePos[1] - lastDown[1]);

            switch (Controls.Mode)
            {
            case Controls.MouseMode::Camera:
                cam.Pos = Camera.DragOrigin + Camera.LIN_MV_SCALE/height() * delta;
                Camera.MustUpdate = true;
                break;

            case Controls.MouseMode::UserSphere:
                Renderer->SetUserSpherePos(UserSphere.DragOrigin - UserSphere.LIN_MV_SCALE/height() * delta);
                break;
            }
        }

        return true;
    }

    /// Zooms in/out with a smooth acceleration/deceleration
    void ProcessZoom()
    {
        double tNow = glfwGetTime();
        float delta = (float)(tNow - Zoom.tLastZoomUpdate);

        if (tNow - Zoom.tLastZoomEvent < Zoom.DELAY_S)
        {
            // Accelerate the zoom rate
            Zoom.speed *= std::pow(Zoom.ACCEL, (Zoom.zoomingIn ? -1 : 1) * delta);
        }
        else if (Zoom.speed != Zoom.SPEED_NONE)
        {
            // Decelerate the zoom rate
            Zoom.speed *= std::pow(Zoom.ACCEL, (Zoom.zoomingIn ? 1 : -1) * delta);

            if (Zoom.zoomingIn != (Zoom.speed < Zoom.SPEED_NONE))
                Zoom.speed = Zoom.SPEED_NONE;
        }

        Zoom.speed = std::min(Zoom.speed, Zoom.MAX_SPEED);
        Zoom.speed = std::max(Zoom.speed, 1/Zoom.MAX_SPEED);

        if (Zoom.speed != Zoom.SPEED_NONE)
        {
            Camera.Cam.FovY *= std::pow(Zoom.speed, delta);
            Camera.Cam.FovY = std::max(Camera.Cam.FovY, Zoom.MIN_FOVY);
            Camera.Cam.FovY = std::min(Camera.Cam.FovY, Zoom.MAX_FOVY);

            Camera.MustUpdate = true;
        }

        Zoom.tLastZoomUpdate = tNow;
    }

    /// Called repeatedly from nanogui::mainloop()
    void drawContents() override
    {
        ProcessZoom();


        if (GUI.pathsChanged
            && Rendering.mode == Rendering.Mode::PathTracing
            && glfwGetTime() - GUI.tLastPathsChange >= GUI.PATH_COUNT_CHANGE_DELAY_SEC)
        {
            UpdatePathTracingProgress(0, Rendering.PathsPerPixel);
            Renderer->RestartPathTracing(Rendering.PathsPerPass, Rendering.PathsPerPixel);

            GUI.pathsChanged = false;
        }

        glFinish();
        double tRenderStart = glfwGetTime();

        if (Camera.MustUpdate)
        {
            Renderer->SetCamera(Camera.Cam);
            Camera.MustUpdate = false;
        }

        switch (Rendering.mode)
        {
            case Rendering.Mode::DirectLighting: Renderer->RenderDirectLighting(); break;

            case Rendering.Mode::PathTracing:

                UpdatePathTracingProgress(Renderer->RenderPathTracingPass(),
                                          Renderer->GetPathsPerPixel());
                break;
        }

        glFinish();

        double tNow = glfwGetTime();
        double renderTime = tNow - tRenderStart;

        if (tNow - TPrevSec >= 1.0)
        {
            TPrevSec = tNow;

            GUI.Info.renderSpeed->setCaption(
                    gpuart::Utils::FormatStr("%.1f ms (%u/s)", renderTime*1000, (unsigned)(1.0/renderTime)).get());
            performLayout();
        }

        glfwPostEmptyEvent();
    }

public:

    GPUARTApp(): nanogui::Screen({ 640, 480 }, "GPU-Assisted Raytracer")
    {
        std::cout << "GL version: " << glGetString(GL_VERSION) << std::endl;
        std::cout << "GL renderer: " << glGetString(GL_RENDERER) << std::endl;
        std::cout << "GLSL version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << "\n" << std::endl;

        Camera.Cam.Pos = Vec3f(0.1, -3.05, 1);
        Camera.Cam.Up = Vec3f(0, 0, 01);
        Camera.Cam.Dir = Vec3f(0, 0, 0.95) - Camera.Cam.Pos;
        Camera.Cam.FovY = 60;
        Camera.Cam.ScreenDist = 0.2f;

        if (!gpuart::GL::Init())
            throw std::runtime_error("Failed to create full quad's vertex buffers");

        Renderer.reset(new gpuart::Renderer(width(), height(), Camera.Cam));
        if (!Renderer->GetIsOK())
            throw std::runtime_error("Renderer initialization failed");

        Renderer->SetUserSphere(Vec3f(-0.4f, 0, 0.2f), 0, 0);
        InitBox(*Renderer);

        InitGUI();
        UpdateScreenSizeInfo(mFBSize[0], mFBSize[1]);

        TPrevSec = glfwGetTime();
        Zoom.tLastZoomEvent = -1.0e+6;
    }
};


// --------------------------------------------------------

int main(int, char **)
{
    try
    {
        nanogui::init();
        std::unique_ptr<GPUARTApp> app(new GPUARTApp());

        nanogui::mainloop(0);

        nanogui::shutdown();
    }
    catch (const std::runtime_error &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
