#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <time.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

#define MAX_VERTEX_BUFFER 512 * 1024
#define MAX_INDEX_BUFFER 128 * 1024

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_D3D11_IMPLEMENTATION

#include "nuklear.h"
#include "nuklear_d3d11.h"

#include "dynamic_array.h"
#include "process.h"
#include "memory.h"
#include "utils.h"

static IDXGISwapChain *swap_chain;
static ID3D11Device *device;
static ID3D11DeviceContext *context;
static ID3D11RenderTargetView *rt_view;

static void
set_swap_chain_size(int width, int height)
{
    ID3D11Texture2D *back_buffer;
    D3D11_RENDER_TARGET_VIEW_DESC desc;
    HRESULT hr;

    if (rt_view)
        ID3D11RenderTargetView_Release(rt_view);

    ID3D11DeviceContext_OMSetRenderTargets(context, 0, NULL, NULL);

    hr = IDXGISwapChain_ResizeBuffers(swap_chain, 0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET || hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR)
    {
        /* to recover from this, you'll need to recreate device and all the resources */
        MessageBoxW(NULL, L"DXGI device is removed or reset!", L"Error", 0);
        exit(0);
    }
    assert(SUCCEEDED(hr));

    memset(&desc, 0, sizeof(desc));
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

    hr = IDXGISwapChain_GetBuffer(swap_chain, 0, &IID_ID3D11Texture2D, (void **)&back_buffer);
    assert(SUCCEEDED(hr));

    hr = ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource *)back_buffer, &desc, &rt_view);
    assert(SUCCEEDED(hr));

    ID3D11Texture2D_Release(back_buffer);
}

static LRESULT CALLBACK
WindowProc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:
        if (swap_chain)
        {
            int width = LOWORD(lparam);
            int height = HIWORD(lparam);
            set_swap_chain_size(width, height);
            nk_d3d11_resize(context, width, height);
        }
        break;
    }

    if (nk_d3d11_handle_event(wnd, msg, wparam, lparam))
        return 0;

    return DefWindowProcW(wnd, msg, wparam, lparam);
}

/* GUI Variables */
static int width;
static int height;
static int selected_row;
static int show_processes_list;

static float modal_width;
static float modal_height;
static float modal_x;
static float modal_y;

char *current_process_name;

/* GUI Functions Declarations */
void show_menubar(struct nk_context *ctx);
void show_combobox(struct nk_context *ctx);
void show_processes_selector(struct nk_context *ctx);
void show_tables(struct nk_context *ctx, ResultsTable *memory_table, SelectionTable *s_table);

/* GUI Functions Definitions */
void show_menubar(struct nk_context *ctx)
{
    // Menubar
    nk_menubar_begin(ctx);

    nk_layout_row_begin(ctx, NK_STATIC, 25, 3);
    nk_layout_row_push(ctx, 60);

    // Process menu
    if (nk_menu_begin_label(ctx, "Process", NK_TEXT_LEFT, nk_vec2(120, 200)))
    {
        nk_layout_row_dynamic(ctx, 25, 1);
        if (nk_menu_item_label(ctx, "Open process", NK_TEXT_LEFT))
        {
            show_processes_list = 1;
        }
        if (nk_menu_item_label(ctx, "Close current", NK_TEXT_LEFT))
        {
            strcpy_s(current_process_name, sizeof(MAX_NAME_LEN), "");
            selected_process = -1;
        }
        nk_menu_end(ctx);
    }

    // Help menu
    if (nk_menu_begin_label(ctx, "Help", NK_TEXT_LEFT, nk_vec2(120, 200)))
    {
        nk_layout_row_dynamic(ctx, 25, 1);
        if (nk_menu_item_label(ctx, "About", NK_TEXT_LEFT))
        {
        }
        if (nk_menu_item_label(ctx, "GitHub", NK_TEXT_LEFT))
        {
        }
        nk_menu_end(ctx);
    }

    // Second row: Centered Process Name
    nk_layout_row_dynamic(ctx, 25, 1);
    nk_label(ctx, current_process_name, NK_TEXT_CENTERED);

    nk_menubar_end(ctx);
}

void show_combobox(struct nk_context *ctx)
{
    nk_layout_row_static(ctx, 25, 200, 2);

    // Scan Type Combobox
    static const char *scan_types[] = {"Exact Value", "Bigger than...", "Smaller than...",
                                       "Value between...", "Unknown initial value"};
    selected_scan_type = nk_combo(ctx, scan_types, NK_LEN(scan_types), selected_scan_type, 25,
                                  nk_vec2(200, 200));

    // Value Type Combobox
    static const char *value_types[] = {"Byte", "2 bytes", "4 bytes", "8 bytes"};
    selected_value_type = nk_combo(ctx, value_types, NK_LEN(value_types), selected_value_type, 25,
                                   nk_vec2(200, 150));

    if (width >= 825)
    {
        nk_layout_row_static(ctx, 25, 200, 4);
    }

    // Value text input
    nk_edit_string(ctx, NK_EDIT_FIELD, search_value, &search_value_len, MAX_NAME_LEN, nk_filter_ascii);

    // Buttons for scan operations
    if (nk_button_label(ctx, "Scan"))
    {
        if (selected_process >= 0 && strlen(search_value) > 0)
        {
            HANDLE process_handle = processes[selected_process].handle;
            start_memory_scan(process_handle, &results_table);
        }
    }
    if (nk_button_label(ctx, "Next Scan"))
    {
        if (selected_process >= 0 && strlen(search_value) > 0)
        {
            HANDLE process_handle = processes[selected_process].handle;
            refine_memory_scan(process_handle, &results_table);
        }
    }
}

void show_tables(struct nk_context *ctx, ResultsTable *r_table, SelectionTable *s_table)
{
    static int context_menu_row = -1;
    static struct nk_vec2 context_menu_pos;

    // Read-only Table
    nk_layout_row_dynamic(ctx, 200, 1);
    if (nk_group_begin(ctx, "Scan Results", NK_WINDOW_BORDER | NK_WINDOW_TITLE))
    {
        nk_layout_row_dynamic(ctx, 25, 3);
        nk_label(ctx, "Address", NK_TEXT_CENTERED);
        nk_label(ctx, "Value", NK_TEXT_CENTERED);
        nk_label(ctx, "Previous Value", NK_TEXT_CENTERED);

        static int selected_row = -1;

        for (size_t i = 0; i < r_table->result_count; i++)
        {
            ResultEntry *entry = &r_table->results[i];
            nk_layout_row_dynamic(ctx, 25, 3);

            nk_bool is_selected = (selected_row == (int)i);

            struct nk_style_selectable selectable_style_backup = ctx->style.selectable;
            struct nk_style_window window_style_backup = ctx->style.window;

            ctx->style.window.spacing = nk_vec2(0, 0);
            ctx->style.window.padding = nk_vec2(0, 0);

            // Modify style for selected row
            if (is_selected)
            {
                ctx->style.selectable.normal = nk_style_item_color(nk_rgb(35, 35, 35)); // Yellow
                ctx->style.selectable.hover = ctx->style.selectable.normal;
            }

            char addr_str[20];
            snprintf(addr_str, sizeof(addr_str), "0x%p", entry->address);

            // Create selectable labels
            nk_bool addr_clicked = nk_selectable_label(ctx, addr_str, NK_TEXT_CENTERED, &is_selected);
            nk_bool value_clicked = nk_selectable_label(ctx, entry->value ? entry->value : "NULL", NK_TEXT_CENTERED, &is_selected);
            nk_bool prev_clicked = nk_selectable_label(ctx, entry->previous_value ? entry->previous_value : "NULL", NK_TEXT_CENTERED, &is_selected);

            // Check for right-clicks on any cell in the row
            if (ctx->input.mouse.buttons[NK_BUTTON_RIGHT].clicked)
            {
                context_menu_row = (int)i;
                context_menu_pos = ctx->input.mouse.pos;
            }

            // Restore original style
            ctx->style.selectable = selectable_style_backup;
            ctx->style.window = window_style_backup;

            // Update selection if any cell was clicked
            if (addr_clicked || value_clicked || prev_clicked)
            {
                selected_row = (int)i;
            }
        }
        nk_group_end(ctx);
    }

    // Display context menu if right-clicked on a row
    if (context_menu_row >= 0)
    {
        // Convert to screen coordinates for popup positioning
        if (nk_popup_begin(ctx, NK_POPUP_DYNAMIC, "row_context_menu",
                           NK_WINDOW_NO_SCROLLBAR,
                           nk_rect(context_menu_pos.x, context_menu_pos.y, 150, 100)))
        {
            nk_layout_row_dynamic(ctx, 25, 1);

            // Menu items - customize these as needed
            if (nk_menu_item_label(ctx, "Copy Address", NK_TEXT_LEFT))
            {
                // TODO : Find why the address copied to clipboard
                // isn't the same as the one display in the table
                char addr_str[20];
                ResultEntry *entry = &r_table->results[context_menu_row];
                printf("Copying to clipboard address: %p", entry->address);
                snprintf(addr_str, sizeof(addr_str), "0x%p", entry->address);
                copy_to_clipboard(addr_str);

                context_menu_row = -1;
                nk_popup_close(ctx);
            }

            if (nk_menu_item_label(ctx, "Copy Value", NK_TEXT_LEFT))
            {
                ResultEntry *entry = &r_table->results[context_menu_row];
                copy_to_clipboard(entry->value);

                context_menu_row = -1;
                nk_popup_close(ctx);
            }

            if (nk_menu_item_label(ctx, "Add to Selection", NK_TEXT_LEFT))
            {
                if (context_menu_row < r_table->result_count && s_table->selection_count < s_table->selection_capacity)
                {
                    SelectionEntry entry = {
                        .address = r_table->results[context_menu_row].address,
                        .freeze = false,
                        .value = r_table->results[context_menu_row].value ? strdup(r_table->results[context_menu_row].value) : strdup("")};

                    if (entry.value)
                    {
                        s_table->selection[s_table->selection_count] = entry;
                        s_table->selection_count++;
                    }
                }

                context_menu_row = -1;
                nk_popup_close(ctx);
            }

            nk_popup_end(ctx);
        }
        else
        {
            // If popup is closed, reset context menu row
            context_menu_row = -1;
        }
    }

    // Editable Table for Selected Addresses
    nk_layout_row_dynamic(ctx, 200, 1);
    if (nk_group_begin(ctx, "Selected Addresses", NK_WINDOW_BORDER | NK_WINDOW_TITLE))
    {
        nk_layout_row_dynamic(ctx, 25, 3);
        nk_label(ctx, "Address", NK_TEXT_CENTERED);
        nk_label(ctx, "Value", NK_TEXT_CENTERED);
        nk_label(ctx, "Freeze", NK_TEXT_CENTERED);

        for (size_t i = 0; i < s_table->selection_count; i++)
        {
            SelectionEntry *entry = &s_table->selection[i];
            nk_layout_row_dynamic(ctx, 25, 3);

            char addr_str[20];
            snprintf(addr_str, sizeof(addr_str), "0x%p", entry->address);
            nk_label(ctx, addr_str, NK_TEXT_CENTERED);

            // Editable Value
            size_t length = strlen(entry->value);
            nk_flags result = nk_edit_string(ctx, NK_EDIT_SIMPLE, entry->value, &length, sizeof(entry->value), nk_filter_ascii);
            entry->value[length] = '\0';

            // Freeze Checkbox
            nk_checkbox_label_align(ctx, "", &entry->freeze, NK_WIDGET_CENTERED, NK_TEXT_CENTERED);
        }
        nk_group_end(ctx);
    }
}

void show_processes_selector(struct nk_context *ctx)
{
    if (show_processes_list)
    {
        if (nk_popup_begin(ctx, NK_POPUP_STATIC, "Processes Selector", NK_WINDOW_CLOSABLE,
                           nk_rect(modal_x, modal_y, modal_width, modal_height)))
        {
            get_running_processes();

            // Dynamic Process List
            nk_layout_row_dynamic(ctx, modal_height - (height / 6), 1);

            // Flexible layout for the list
            if (nk_group_begin(ctx, "Process List", NK_WINDOW_BORDER))
            {
                for (int i = 0; i < process_count; ++i)
                {
                    nk_layout_row_dynamic(ctx, 30, 1);
                    if (nk_select_label(ctx, processes[i].name, NK_TEXT_LEFT, selected_process == i))
                    {
                        selected_process = i;
                    }
                }
                nk_group_end(ctx);
            }

            // "Open" Button
            nk_layout_row_dynamic(ctx, 30, 1);
            if (nk_button_label(ctx, "Open"))
            {
                if (selected_process >= 0)
                {
                    strncpy_s(current_process_name, MAX_NAME_LEN, processes[selected_process].name, _TRUNCATE);
                }
                show_processes_list = 0;
                nk_popup_close(ctx);
            }

            nk_popup_end(ctx);
        }
        else
        {
            show_processes_list = 0; // Hide modal
        }
    }
}

int main(void)
{
    struct nk_context *ctx;
    struct nk_colorf bg;

    WNDCLASSW wc;
    RECT rect = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD exstyle = WS_EX_APPWINDOW;
    HWND wnd;
    int running = 1;
    HRESULT hr;
    D3D_FEATURE_LEVEL feature_level;
    DXGI_SWAP_CHAIN_DESC swap_chain_desc;

    /* Win32 */
    memset(&wc, 0, sizeof(wc));
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandleW(0);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"ShadowEngineClass";
    RegisterClassW(&wc);

    AdjustWindowRectEx(&rect, style, FALSE, exstyle);

    wnd = CreateWindowExW(exstyle, wc.lpszClassName, L"Shadow Engine",
                          style | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
                          rect.right - rect.left, rect.bottom - rect.top,
                          NULL, NULL, wc.hInstance, NULL);

    /* D3D11 setup */
    memset(&swap_chain_desc, 0, sizeof(swap_chain_desc));
    swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_desc.BufferDesc.RefreshRate.Numerator = 60;
    swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.SampleDesc.Quality = 0;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.BufferCount = 1;
    swap_chain_desc.OutputWindow = wnd;
    swap_chain_desc.Windowed = TRUE;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    swap_chain_desc.Flags = 0;

    if (FAILED(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE,
                                             NULL, 0, NULL, 0, D3D11_SDK_VERSION, &swap_chain_desc,
                                             &swap_chain, &device, &feature_level, &context)))
    {
        /* if hardware device fails, then try WARP high-performance
           software rasterizer, this is useful for RDP sessions */
        hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_WARP,
                                           NULL, 0, NULL, 0, D3D11_SDK_VERSION, &swap_chain_desc,
                                           &swap_chain, &device, &feature_level, &context);
        assert(SUCCEEDED(hr));
    }
    set_swap_chain_size(WINDOW_WIDTH, WINDOW_HEIGHT);

    /* GUI */
    ctx = nk_d3d11_init(device, WINDOW_WIDTH, WINDOW_HEIGHT, MAX_VERTEX_BUFFER, MAX_INDEX_BUFFER);
    /* Load Fonts: if none of these are loaded a default font will be used  */
    /* Load Cursor: if you uncomment cursor loading please hide the cursor */
    {
        struct nk_font_atlas *atlas;
        nk_d3d11_font_stash_begin(&atlas);
        // struct nk_font *roboto = nk_font_atlas_add_from_file(atlas, "../fonts/ProggyClean.ttf", 14, 0);
        nk_d3d11_font_stash_end();
        // nk_style_load_all_cursors(ctx, atlas->cursors);
        // nk_style_set_font(ctx, &roboto->handle);
    }

    selected_row = -1;
    show_processes_list = 0;
    current_process_name = malloc(MAX_NAME_LEN);

    create_array(&memory_addresses, 100000, sizeof(LPVOID));
    init_selection_table(&selection_table);
    init_results_table(&results_table);

    bg.r = 0.10f, bg.g = 0.18f, bg.b = 0.24f, bg.a = 1.0f;
    while (running)
    {
        /* Input */
        MSG msg;
        nk_input_begin(ctx);
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                running = 0;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        nk_input_end(ctx);

        RECT client_rect;
        GetClientRect(wnd, &client_rect);
        width = (int)client_rect.right - (int)client_rect.left;
        height = (int)client_rect.bottom - (int)client_rect.top;

        modal_width = ((float)width / 2);
        modal_height = ((float)height / 2);
        modal_x = (width - modal_width) / 2;
        modal_y = (height - modal_height) / 2;

        /* GUI */
        if (nk_begin(ctx, "Shadow Engine", nk_rect(0, 0, (float)width, (float)height),
                     NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR))
        {
            show_menubar(ctx);
            show_combobox(ctx);
            show_tables(ctx, &results_table, &selection_table);

            show_processes_selector(ctx);
        }
        nk_end(ctx);

        /* Draw */
        ID3D11DeviceContext_ClearRenderTargetView(context, rt_view, &bg.r);
        ID3D11DeviceContext_OMSetRenderTargets(context, 1, &rt_view, NULL);
        nk_d3d11_render(context, NK_ANTI_ALIASING_ON);
        hr = IDXGISwapChain_Present(swap_chain, 1, 0);
        if (hr == DXGI_ERROR_DEVICE_RESET || hr == DXGI_ERROR_DEVICE_REMOVED)
        {
            MessageBoxW(NULL, L"D3D11 device is lost or removed!", L"Error", 0);
            break;
        }
        else if (hr == DXGI_STATUS_OCCLUDED)
        {
            Sleep(10);
        }
        assert(SUCCEEDED(hr));
    }

    free_array(&memory_addresses);
    cleanup_process_handles();

    ID3D11DeviceContext_ClearState(context);
    nk_d3d11_shutdown();
    ID3D11RenderTargetView_Release(rt_view);
    ID3D11DeviceContext_Release(context);
    ID3D11Device_Release(device);
    IDXGISwapChain_Release(swap_chain);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}
