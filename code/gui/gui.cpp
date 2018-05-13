#include "gui.h"
#include <rdi_backend\rdi_backend_dx11.h>
#include <window\window_interface.h>
#include <window\window.h>

#include <3rd_party\imgui\imgui.h>
#include <3rd_party\imgui\dx11\imgui_impl_dx11.h>

extern LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
static int GuiWindowCallback(uintptr_t hwnd, uint32_t msg, uintptr_t wparam, uintptr_t lparam, void* userdata)
{
    return (int)ImGui_ImplWin32_WndProcHandler( (HWND)hwnd, msg, wparam, lparam );
}

void GUI::StartUp( BXIWindow* window_plugin, RDIDevice* rdidev )
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    {
        const BXWindow* window = window_plugin->GetWindow();
        void* hwnd = (void*)window->GetSystemHandle( window );

        ID3D11Device* apiDev = nullptr;
        ID3D11DeviceContext* apiCtx = nullptr;
        GetAPIDevice( rdidev, &apiDev, &apiCtx );

        bool bres = ImGui_ImplDX11_Init( hwnd, apiDev, apiCtx );
        SYS_ASSERT( bres );
    }

    window_plugin->AddCallback( GuiWindowCallback, nullptr );
}

void GUI::ShutDown()
{
    ImGui_ImplDX11_Shutdown();
    ImGui::DestroyContext();
}

void GUI::NewFrame()
{
    ImGui_ImplDX11_NewFrame();
}

void GUI::Draw()
{
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData( ImGui::GetDrawData() );
}

