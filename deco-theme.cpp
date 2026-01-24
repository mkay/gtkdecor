#include "deco-theme.hpp"
#include <wayfire/core.hpp>
#include <wayfire/opengl.hpp>
#include <config.h>
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <sys/stat.h>

#ifdef HAVE_LIBRSVG
#include <librsvg/rsvg.h>
#endif

namespace wf
{
namespace decor
{
/** Parse CSS color string to wf::color_t */
wf::color_t decoration_theme_t::parse_css_color(const std::string& color_str) const
{
    wf::color_t color{0, 0, 0, 1.0};

    // Match hex color: #rrggbb or #rrggbbaa
    std::regex hex_regex("#([0-9a-fA-F]{6})([0-9a-fA-F]{2})?");
    std::smatch hex_match;
    if (std::regex_search(color_str, hex_match, hex_regex))
    {
        std::string hex = hex_match[1].str();
        color.r = std::stoi(hex.substr(0, 2), nullptr, 16) / 255.0;
        color.g = std::stoi(hex.substr(2, 2), nullptr, 16) / 255.0;
        color.b = std::stoi(hex.substr(4, 2), nullptr, 16) / 255.0;

        if (hex_match[2].matched)
        {
            color.a = std::stoi(hex_match[2].str(), nullptr, 16) / 255.0;
        } else
        {
            color.a = 1.0;
        }
        return color;
    }

    // Match rgba: rgba(r, g, b, a)
    std::regex rgba_regex(R"(rgba?\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*(?:,\s*([\d.]+))?\s*\))");
    std::smatch rgba_match;
    if (std::regex_search(color_str, rgba_match, rgba_regex))
    {
        color.r = std::stoi(rgba_match[1].str()) / 255.0;
        color.g = std::stoi(rgba_match[2].str()) / 255.0;
        color.b = std::stoi(rgba_match[3].str()) / 255.0;
        color.a = rgba_match[4].matched ? std::stod(rgba_match[4].str()) : 1.0;
        return color;
    }

    return color;  // Return black if parsing fails
}

/** Get current GTK theme name from settings */
std::string decoration_theme_t::get_gtk_theme_name() const
{
    const char* home = getenv("HOME");
    if (!home)
    {
        return "";
    }

    std::string settings_path = std::string(home) + "/.config/gtk-3.0/settings.ini";
    std::ifstream settings_file(settings_path);
    if (!settings_file.is_open())
    {
        return "";
    }

    std::string line;
    while (std::getline(settings_file, line))
    {
        if (line.find("gtk-theme-name=") == 0)
        {
            return line.substr(15);  // Length of "gtk-theme-name="
        }
    }

    return "";
}

/** Get current icon theme name from settings */
std::string decoration_theme_t::get_icon_theme_name() const
{
    const char* home = getenv("HOME");
    if (!home)
    {
        return "hicolor"; // Default fallback
    }

    std::string settings_path = std::string(home) + "/.config/gtk-3.0/settings.ini";
    std::ifstream settings_file(settings_path);
    if (!settings_file.is_open())
    {
        return "hicolor";
    }

    std::string line;
    while (std::getline(settings_file, line))
    {
        if (line.find("gtk-icon-theme-name=") == 0)
        {
            return line.substr(20);  // Length of "gtk-icon-theme-name="
        }
    }

    return "hicolor";
}

/** Get GTK font name from settings */
std::string decoration_theme_t::get_gtk_font_name() const
{
    const char* home = getenv("HOME");
    if (!home)
    {
        return "";
    }

    std::string settings_path = std::string(home) + "/.config/gtk-3.0/settings.ini";
    std::ifstream settings_file(settings_path);
    if (!settings_file.is_open())
    {
        return "";
    }

    std::string line;
    while (std::getline(settings_file, line))
    {
        if (line.find("gtk-font-name=") == 0)
        {
            return line.substr(14);  // Length of "gtk-font-name="
        }
    }

    return "";
}

/** Find theme CSS file path */
std::string decoration_theme_t::find_theme_css_file(const std::string& theme_name) const
{
    if (theme_name.empty())
    {
        return "";
    }

    const char* home = getenv("HOME");
    std::vector<std::string> search_paths;

    if (home)
    {
        search_paths.push_back(std::string(home) + "/.themes/" + theme_name + "/gtk-3.0/gtk.css");
        search_paths.push_back(std::string(home) + "/.themes/" + theme_name + "/gtk-3.0/gtk-dark.css");
    }

    search_paths.push_back("/usr/share/themes/" + theme_name + "/gtk-3.0/gtk.css");
    search_paths.push_back("/usr/share/themes/" + theme_name + "/gtk-3.0/gtk-dark.css");
    search_paths.push_back("/usr/local/share/themes/" + theme_name + "/gtk-3.0/gtk.css");

    for (const auto& path : search_paths)
    {
        struct stat buffer;
        if (stat(path.c_str(), &buffer) == 0)
        {
            return path;
        }
    }

    return "";
}

/** Find icon file path in icon theme */
std::string decoration_theme_t::find_icon_file(const std::string& icon_name, int size) const
{
    if (icon_theme_name.empty() || icon_name.empty())
    {
        return "";
    }

    const char* home = getenv("HOME");
    std::vector<std::string> base_paths;

    if (home)
    {
        base_paths.push_back(std::string(home) + "/.icons");
        base_paths.push_back(std::string(home) + "/.local/share/icons");
    }
    base_paths.push_back("/usr/share/icons");
    base_paths.push_back("/usr/local/share/icons");

    // Common size directories to check
    std::vector<std::string> size_dirs = {
        "symbolic",
        std::to_string(size) + "x" + std::to_string(size),
        "scalable",
        "16x16", "22x22", "24x24", "32x32", "48x48"
    };

    // Common subdirectories for window controls
    std::vector<std::string> sub_dirs = {
        "actions", "ui", "status", "places"
    };

    for (const auto& base : base_paths)
    {
        for (const auto& size_dir : size_dirs)
        {
            for (const auto& sub_dir : sub_dirs)
            {
                // Try SVG first
                std::string path = base + "/" + icon_theme_name + "/" + size_dir + "/" + sub_dir + "/" + icon_name + ".svg";
                struct stat buffer;
                if (stat(path.c_str(), &buffer) == 0)
                {
                    return path;
                }

                // Try PNG
                path = base + "/" + icon_theme_name + "/" + size_dir + "/" + sub_dir + "/" + icon_name + ".png";
                if (stat(path.c_str(), &buffer) == 0)
                {
                    return path;
                }
            }

            // Also try without subdirectory
            std::string path = base + "/" + icon_theme_name + "/" + size_dir + "/" + icon_name + ".svg";
            struct stat buffer;
            if (stat(path.c_str(), &buffer) == 0)
            {
                return path;
            }

            path = base + "/" + icon_theme_name + "/" + size_dir + "/" + icon_name + ".png";
            if (stat(path.c_str(), &buffer) == 0)
            {
                return path;
            }
        }
    }

    // Fallback to Adwaita if icon not found in theme
    if (icon_theme_name != "Adwaita")
    {
        for (const auto& base : base_paths)
        {
            std::string path = base + "/Adwaita/symbolic/ui/" + icon_name + ".svg";
            struct stat buffer;
            if (stat(path.c_str(), &buffer) == 0)
            {
                return path;
            }
        }
    }

    return "";
}

/** Parse theme CSS file and extract colors */
void decoration_theme_t::parse_theme_css(const std::string& css_file) const
{
    std::ifstream file(css_file);
    if (!file.is_open())
    {
        LOGE("Failed to open theme CSS file: ", css_file);
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string css_content = buffer.str();

    // Build map of @define-color variables
    std::map<std::string, std::string> color_vars;
    std::regex define_color_regex(R"(@define-color\s+(\w+)\s+([^;]+);)");
    std::smatch match;
    std::string::const_iterator searchStart(css_content.cbegin());
    while (std::regex_search(searchStart, css_content.cend(), match, define_color_regex))
    {
        color_vars[match[1].str()] = match[2].str();
        searchStart = match.suffix().first;
    }

    // Try to find background color from @define-color theme_bg_color or theme_unfocused_bg_color
    if (color_vars.count("theme_bg_color"))
    {
        theme_titlebar_bg_active = parse_css_color(color_vars["theme_bg_color"]);
        LOGI("Using theme_bg_color for titlebar background");
    }

    if (color_vars.count("theme_unfocused_bg_color"))
    {
        theme_titlebar_bg_inactive = parse_css_color(color_vars["theme_unfocused_bg_color"]);
    } else
    {
        theme_titlebar_bg_inactive = theme_titlebar_bg_active;
    }

    // Try to find foreground/text color
    if (color_vars.count("theme_fg_color"))
    {
        theme_titlebar_fg_active = parse_css_color(color_vars["theme_fg_color"]);
        LOGI("Using theme_fg_color for titlebar text");
    }

    if (color_vars.count("theme_unfocused_fg_color"))
    {
        theme_titlebar_fg_inactive = parse_css_color(color_vars["theme_unfocused_fg_color"]);
    } else if (color_vars.count("unfocused_insensitive_color"))
    {
        theme_titlebar_fg_inactive = parse_css_color(color_vars["unfocused_insensitive_color"]);
    } else
    {
        theme_titlebar_fg_inactive = theme_titlebar_fg_active;
    }

    // Set button colors - matching GTK style (visible circular backgrounds)
    theme_button_bg = wf::color_t{0.4, 0.4, 0.4, 0.3};  // Gray circle in normal state
    theme_button_hover_bg = wf::color_t{0.5, 0.5, 0.5, 0.4};  // Lighter on hover
    theme_button_active_bg = wf::color_t{0.3, 0.3, 0.3, 0.5};  // Darker when pressed

    // Try to find font settings - look for window.titlebar or headerbar font
    std::regex font_regex(R"(font-family:\s*([^;]+);)");
    std::regex font_size_regex(R"(font-size:\s*(\d+)(?:px|pt)?)");
    std::smatch font_match;

    // Search for headerbar or window titlebar font settings
    std::string::const_iterator font_search_start(css_content.cbegin());
    if (std::regex_search(font_search_start, css_content.cend(), font_match, font_regex))
    {
        theme_font_family = font_match[1].str();
        // Remove quotes if present
        theme_font_family.erase(std::remove(theme_font_family.begin(), theme_font_family.end(), '"'),
                                theme_font_family.end());
        theme_font_family.erase(std::remove(theme_font_family.begin(), theme_font_family.end(), '\''),
                                theme_font_family.end());
        LOGI("Found theme font family: ", theme_font_family);
    }

    if (std::regex_search(css_content, font_match, font_size_regex))
    {
        theme_font_size = std::stoi(font_match[1].str());
        LOGI("Found theme font size: ", theme_font_size);
    }

    LOGI("Parsed GTK theme colors from: ", css_file);
    LOGI("Titlebar bg active: rgba(",
         int(theme_titlebar_bg_active.r * 255), ", ",
         int(theme_titlebar_bg_active.g * 255), ", ",
         int(theme_titlebar_bg_active.b * 255), ", ",
         theme_titlebar_bg_active.a, ")");
}

/** Load GTK theme by parsing CSS files */
void decoration_theme_t::load_gtk_theme() const
{
    if (theme_loaded)
    {
        return;
    }

    theme_loaded = true;  // Mark as attempted even if it fails

    // Get icon theme name from GTK settings
    icon_theme_name = get_icon_theme_name();
    LOGI("Found icon theme: ", icon_theme_name);

    // Get font name from GTK settings
    std::string gtk_font = get_gtk_font_name();
    if (!gtk_font.empty())
    {
        // Parse font string (format: "Font Name Size")
        size_t last_space = gtk_font.rfind(' ');
        if (last_space != std::string::npos)
        {
            theme_font_family = gtk_font.substr(0, last_space);
            try {
                theme_font_size = std::stoi(gtk_font.substr(last_space + 1));
            } catch (...) {
                theme_font_size = 11; // Default size
            }
            LOGI("Found GTK font: ", theme_font_family, " ", theme_font_size);
        }
    }

    // Get theme name from GTK settings
    std::string theme_name = get_gtk_theme_name();
    if (theme_name.empty())
    {
        LOGE("Could not determine GTK theme name, using fallback colors");
        return;
    }

    LOGI("Found GTK theme: ", theme_name);

    // Find theme CSS file
    std::string css_file = find_theme_css_file(theme_name);
    if (css_file.empty())
    {
        LOGE("Could not find CSS file for theme: ", theme_name);
        return;
    }

    LOGI("Loading theme CSS from: ", css_file);

    // Parse the CSS file
    parse_theme_css(css_file);
}

/** Create a new theme with the default parameters */
decoration_theme_t::decoration_theme_t() :
    theme_loaded(false)
{
    // Theme loading is deferred until first render (lazy initialization)
    // Initialize with fallback colors in case theme loading fails
    theme_titlebar_bg_active = wf::color_t{0.1, 0.1, 0.15, 1.0};
    theme_titlebar_bg_inactive = wf::color_t{0.15, 0.15, 0.2, 1.0};
    theme_titlebar_fg_active = wf::color_t{0.9, 0.9, 0.93, 1.0};
    theme_titlebar_fg_inactive = wf::color_t{0.7, 0.7, 0.73, 0.7};
    theme_button_bg = wf::color_t{0.4, 0.4, 0.4, 0.3};  // Gray circle
    theme_button_hover_bg = wf::color_t{0.5, 0.5, 0.5, 0.4};  // Lighter on hover
    theme_button_active_bg = wf::color_t{0.3, 0.3, 0.3, 0.5};  // Darker when pressed
    theme_font_family = "";
    theme_font_size = 0;
}

decoration_theme_t::~decoration_theme_t()
{
    // No cleanup needed for CSS parsing
}

/** Force reload of theme - call when GTK theme/icon theme changes */
void decoration_theme_t::reload_theme() const
{
    LOGI("Reloading GTK theme and icons");
    theme_loaded = false;
    icon_theme_name.clear();
    theme_font_family.clear();
    theme_font_size = 0;
    // Reset colors to defaults - they'll be reloaded
    theme_titlebar_bg_active = wf::color_t{0.1, 0.1, 0.15, 1.0};
    theme_titlebar_bg_inactive = wf::color_t{0.15, 0.15, 0.2, 1.0};
    theme_titlebar_fg_active = wf::color_t{0.9, 0.9, 0.93, 1.0};
    theme_titlebar_fg_inactive = wf::color_t{0.7, 0.7, 0.73, 0.7};
}

/** @return The available height for displaying the title */
int decoration_theme_t::get_title_height() const
{
    return title_height;
}

/** @return The available border for resizing */
int decoration_theme_t::get_border_size() const
{
    return border_size;
}

/** @return The corner radius for rounded titlebar */
int decoration_theme_t::get_corner_radius() const
{
    return corner_radius;
}

/** @return The available border for resizing */
void decoration_theme_t::set_buttons(button_type_t flags)
{
    button_flags = flags;
}

/**
 * Fill the given rectangle with the background color(s).
 *
 * @param data The render data (pass, target, damage)
 * @param rectangle The rectangle to redraw.
 * @param active Whether to use active or inactive colors
 */
void decoration_theme_t::render_background(const wf::scene::render_instruction_t& data,
    wf::geometry_t rectangle, bool active) const
{
    // Lazy initialization: load GTK theme on first render
    load_gtk_theme();

    // Use theme colors if available, otherwise fall back to config colors
    wf::color_t bg_color = active ? theme_titlebar_bg_active : theme_titlebar_bg_inactive;

    // If theme colors are still default (black), use config colors as ultimate fallback
    if (bg_color.r == 0.0 && bg_color.g == 0.0 && bg_color.b == 0.0)
    {
        bg_color = active ? active_color : inactive_color;
    }

    // Calculate titlebar area (top portion with rounded corners)
    int titlebar_h = title_height + border_size;
    wf::geometry_t titlebar_rect = {
        rectangle.x, rectangle.y,
        rectangle.width, titlebar_h
    };

    // Render titlebar with rounded top corners using Cairo
    auto titlebar_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
        titlebar_rect.width, titlebar_rect.height);
    auto cr = cairo_create(titlebar_surface);

    // Clear surface to transparent
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    // Create rounded rectangle path (only top corners rounded)
    double radius = corner_radius;
    double x = 0, y = 0;
    double w = titlebar_rect.width, h = titlebar_rect.height;

    // Draw path: top-left corner -> top-right corner -> bottom-right -> bottom-left -> close
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + radius, y + radius, radius, M_PI, 3 * M_PI / 2); // Top-left arc
    cairo_arc(cr, x + w - radius, y + radius, radius, -M_PI / 2, 0);    // Top-right arc
    cairo_line_to(cr, x + w, y + h);  // Line to bottom-right
    cairo_line_to(cr, x, y + h);      // Line to bottom-left
    cairo_close_path(cr);

    cairo_set_source_rgba(cr, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
    cairo_fill(cr);

    cairo_destroy(cr);

    // Convert to texture and render
    wf::owned_texture_t titlebar_tex{titlebar_surface};
    data.pass->add_texture(titlebar_tex.get_texture(), data.target, titlebar_rect, data.damage);
    cairo_surface_destroy(titlebar_surface);

    // Render left border
    if (border_size > 0)
    {
        wf::geometry_t left_border = {
            rectangle.x, rectangle.y + titlebar_h,
            border_size, rectangle.height - titlebar_h
        };
        data.pass->add_rect(bg_color, data.target, left_border, data.damage);
    }

    // Render right border
    if (border_size > 0)
    {
        wf::geometry_t right_border = {
            rectangle.x + rectangle.width - border_size, rectangle.y + titlebar_h,
            border_size, rectangle.height - titlebar_h
        };
        data.pass->add_rect(bg_color, data.target, right_border, data.damage);
    }

    // Render bottom border
    if (border_size > 0)
    {
        wf::geometry_t bottom_border = {
            rectangle.x, rectangle.y + rectangle.height - border_size,
            rectangle.width, border_size
        };
        data.pass->add_rect(bg_color, data.target, bottom_border, data.damage);
    }
}

/**
 * Render the given text on a cairo_surface_t with the given size.
 * The caller is responsible for freeing the memory afterwards.
 */
cairo_surface_t*decoration_theme_t::render_text(std::string text,
    int width, int height) const
{
    const auto format = CAIRO_FORMAT_ARGB32;
    auto surface = cairo_image_surface_create(format, width, height);

    if (height == 0)
    {
        return surface;
    }

    wf::color_t color = theme_titlebar_fg_active;
    // Use config font color as fallback
    if (color.r == 0.0 && color.g == 0.0 && color.b == 0.0)
    {
        color = font_color;
    }

    auto cr = cairo_create(surface);

    PangoFontDescription *font_desc;
    PangoLayout *layout;

    // Prefer GTK theme font, fall back to config font
    if (!theme_font_family.empty() && theme_font_size > 0)
    {
        // Use theme font - titlebar fonts are typically smaller than UI fonts
        font_desc = pango_font_description_new();
        pango_font_description_set_family(font_desc, theme_font_family.c_str());
        // Use 0.8x of the GTK font size for titlebar
        pango_font_description_set_size(font_desc, (theme_font_size * 0.8) * PANGO_SCALE);
        pango_font_description_set_weight(font_desc, PANGO_WEIGHT_NORMAL);
    }
    else if (!theme_font_family.empty())
    {
        // Use theme font family with calculated size
        font_desc = pango_font_description_new();
        pango_font_description_set_family(font_desc, theme_font_family.c_str());
        const float font_scale = 0.5; // Reduced from 0.9 for smaller text
        const float font_size  = height * font_scale;
        pango_font_description_set_absolute_size(font_desc, font_size * PANGO_SCALE);
    }
    else
    {
        // Parse font description from config (includes size)
        font_desc = pango_font_description_from_string(((std::string)font).c_str());

        // Only override size if not specified in font string
        if (pango_font_description_get_size(font_desc) == 0)
        {
            const float font_scale = 0.5; // Reduced from 0.9 for smaller text
            const float font_size  = height * font_scale;
            pango_font_description_set_absolute_size(font_desc, font_size * PANGO_SCALE);
        }
    }

    layout = pango_cairo_create_layout(cr);
    pango_layout_set_font_description(layout, font_desc);
    pango_layout_set_text(layout, text.c_str(), text.size());

    // Center the text horizontally
    pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
    pango_layout_set_width(layout, width * PANGO_SCALE);

    // Center vertically
    PangoRectangle ink_rect, logical_rect;
    pango_layout_get_pixel_extents(layout, &ink_rect, &logical_rect);
    int text_y = (height - logical_rect.height) / 2 - logical_rect.y;

    cairo_move_to(cr, 0, text_y);
    cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
    pango_cairo_show_layout(cr, layout);

    pango_font_description_free(font_desc);
    g_object_unref(layout);
    cairo_destroy(cr);

    return surface;
}

cairo_surface_t*decoration_theme_t::get_button_surface(button_type_t button,
    const button_state_t& state) const
{
    // Lazy initialization: load GTK theme on first render
    load_gtk_theme();

    cairo_surface_t *button_surface = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, state.width, state.height);

    auto cr = cairo_create(button_surface);
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);

    /* Clear the button background */
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_rectangle(cr, 0, 0, state.width, state.height);
    cairo_fill(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    // Determine button background color based on hover/active state
    wf::color_t bg_color = theme_button_bg;
    if (state.hover_progress > 0)
    {
        // Hover state
        bg_color = theme_button_hover_bg;
    }
    else if (state.hover_progress < 0)
    {
        // Active/pressed state
        bg_color = theme_button_active_bg;
    }

    // Draw circular button background - GTK style
    // Larger circle so there's visible padding between icon and circle edge
    double radius = (state.width / 2.0) - 2.5;
    cairo_arc(cr, state.width / 2, state.height / 2, radius, 0, 2 * M_PI);
    cairo_set_source_rgba(cr, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
    cairo_fill(cr);

    // Use theme text color for icon, with fallback
    wf::color_t icon_color = theme_titlebar_fg_active;
    if (icon_color.r == 0.0 && icon_color.g == 0.0 && icon_color.b == 0.0)
    {
        icon_color = font_color;  // Fallback to config color
    }

    // Apply transparency based on hover state - semi-transparent by default, opaque on hover
    if (state.hover_progress > 0)
    {
        // Hovered: full opacity white
        icon_color.r = icon_color.g = icon_color.b = 1.0;
        icon_color.a = 1.0;
    }
    else
    {
        // Default: semi-transparent
        icon_color.a = 0.7;
    }

    // Try to load icon from theme
    std::string icon_name;
    switch (button)
    {
      case BUTTON_CLOSE:
        icon_name = "window-close-symbolic";
        break;
      case BUTTON_TOGGLE_MAXIMIZE:
        icon_name = "window-maximize-symbolic";
        break;
      case BUTTON_MINIMIZE:
        icon_name = "window-minimize-symbolic";
        break;
      default:
        break;
    }

    std::string icon_path = find_icon_file(icon_name, static_cast<int>(state.width));
    bool icon_loaded = false;

#ifdef HAVE_LIBRSVG
    if (!icon_path.empty() && icon_path.find(".svg") != std::string::npos)
    {
        GError *error = NULL;
        RsvgHandle *handle = rsvg_handle_new_from_file(icon_path.c_str(), &error);

        if (handle)
        {
            // Create temporary surface for the SVG
            cairo_surface_t *icon_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                state.width, state.height);
            cairo_t *icon_cr = cairo_create(icon_surface);

            // Render SVG with padding inside the button circle
            double icon_size = state.width * 0.83;  // Match GTK size with visible padding
            double offset = (state.width - icon_size) / 2.0;
            RsvgRectangle viewport = {
                .x = offset,
                .y = offset,
                .width = icon_size,
                .height = icon_size
            };

            // Render SVG to temporary surface
            if (rsvg_handle_render_document(handle, icon_cr, &viewport, &error))
            {
                // Use the SVG as a mask and paint with the icon color
                cairo_set_source_rgba(cr, icon_color.r, icon_color.g, icon_color.b, icon_color.a);
                cairo_mask_surface(cr, icon_surface, 0, 0);
                icon_loaded = true;
            }

            cairo_destroy(icon_cr);
            cairo_surface_destroy(icon_surface);
            g_object_unref(handle);
        }

        if (error)
        {
            g_error_free(error);
        }
    }
#endif

    // Fallback to drawing icons if loading failed
    if (!icon_loaded)
    {
        cairo_set_line_width(cr, 1.0 * state.border);
        cairo_set_source_rgba(cr, icon_color.r, icon_color.g, icon_color.b, icon_color.a);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

        switch (button)
        {
          case BUTTON_CLOSE:
            // X icon - much smaller with substantial padding like GTK
            cairo_move_to(cr, state.width * 0.49, state.height * 0.49);
            cairo_line_to(cr, state.width * 0.51, state.height * 0.51);
            cairo_move_to(cr, state.width * 0.51, state.height * 0.49);
            cairo_line_to(cr, state.width * 0.49, state.height * 0.51);
            cairo_stroke(cr);
            break;

          case BUTTON_TOGGLE_MAXIMIZE:
            // Square icon - much smaller with substantial padding like GTK
            {
                double size = state.width * 0.035;
                double x = (state.width - size) / 2.0;
                double y = (state.height - size) / 2.0;
                cairo_rectangle(cr, x, y, size, size);
                cairo_stroke(cr);
            }
            break;

          case BUTTON_MINIMIZE:
            // Minus icon - much smaller with substantial padding like GTK
            cairo_move_to(cr, state.width * 0.49, state.height * 0.5);
            cairo_line_to(cr, state.width * 0.51, state.height * 0.5);
            cairo_stroke(cr);
            break;

          default:
            assert(false);
        }
    }

    cairo_destroy(cr);
    return button_surface;
}
}
}
