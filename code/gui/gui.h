#pragma once

struct BXIWindow;
struct RDIDevice;

struct GUI
{
    static void StartUp( BXIWindow* window_plugin, RDIDevice* rdidev );
    static void ShutDown();

    static void NewFrame();
    static void Draw();
};
