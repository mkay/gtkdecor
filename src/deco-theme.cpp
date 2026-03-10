#include "deco-theme.hpp"
#include <wayfire/core.hpp>
#include <wayfire/opengl.hpp>
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
        // cppcheck-suppress stlIfStrFind
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
        // cppcheck-suppress stlIfStrFind
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
        // cppcheck-suppress stlIfStrFind
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
                // Try both possible directory orderings for maximum compatibility
                // Pattern 1: theme/size/subdir/icon (e.g., Adwaita/symbolic/ui/icon.svg)
                std::string path = base + "/" + icon_theme_name + "/" + size_dir + "/" + sub_dir + "/" + icon_name + ".svg";
                struct stat buffer;
                if (stat(path.c_str(), &buffer) == 0)
                {
                    return path;
                }

                // Pattern 2: theme/subdir/size/icon (e.g., elementary/actions/symbolic/icon.svg)
                path = base + "/" + icon_theme_name + "/" + sub_dir + "/" + size_dir + "/" + icon_name + ".svg";
                if (stat(path.c_str(), &buffer) == 0)
                {
                    return path;
                }

                // Try PNG for both patterns
                path = base + "/" + icon_theme_name + "/" + size_dir + "/" + sub_dir + "/" + icon_name + ".png";
                if (stat(path.c_str(), &buffer) == 0)
                {
                    return path;
                }

                path = base + "/" + icon_theme_name + "/" + sub_dir + "/" + size_dir + "/" + icon_name + ".png";
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
    LOGI("GTK font from settings.ini: '", gtk_font, "'");

    // Store whether we got font from GTK settings (to prevent CSS from overwriting it)
    bool gtk_font_found = false;
    if (!gtk_font.empty())
    {
        // Parse font string (format: "Font Name Size")
        size_t last_space = gtk_font.rfind(' ');
        if (last_space != std::string::npos)
        {
            theme_font_family = gtk_font.substr(0, last_space);
            try {
                theme_font_size = std::stoi(gtk_font.substr(last_space + 1));
                gtk_font_found = true;  // Successfully parsed GTK font
            } catch (...) {
                theme_font_size = 11; // Default size
            }
            LOGI("Parsed font family: '", theme_font_family, "', size: ", theme_font_size);
        }
    }

    // Get theme name from GTK settings
    std::string theme_name = get_gtk_theme_name();
    if (theme_name.empty())
    {
        LOGE("Could not determine GTK theme name, using fallback colors");
        // Set default font if not found from GTK settings
        if (!gtk_font_found)
        {
            theme_font_family = "Sans";
            theme_font_size = 10;
        }
        return;
    }

    LOGI("Found GTK theme: ", theme_name);

    // Find theme CSS file
    std::string css_file = find_theme_css_file(theme_name);
    if (css_file.empty())
    {
        LOGE("Could not find CSS file for theme: ", theme_name);
        // Set default font if not found from GTK settings
        if (!gtk_font_found)
        {
            theme_font_family = "Sans";
            theme_font_size = 10;
        }
        return;
    }

    LOGI("Loading theme CSS from: ", css_file);

    // Save GTK font before parsing CSS (CSS might overwrite it)
    std::string saved_font_family = theme_font_family;
    int saved_font_size = theme_font_size;

    // Parse the CSS file
    parse_theme_css(css_file);

    // Restore GTK font if it was found (prioritize settings.ini over CSS)
    if (gtk_font_found)
    {
        theme_font_family = saved_font_family;
        theme_font_size = saved_font_size;
    }
    // Otherwise use CSS font if available, or default
    else if (theme_font_family.empty())
    {
        theme_font_family = "Sans";
        theme_font_size = 10;
    }
}

/** Create a new theme with the default parameters */
decoration_theme_t::decoration_theme_t() :
    button_flags{},
    theme_loaded(false),
    theme_titlebar_bg_active{0.1, 0.1, 0.15, 1.0},
    theme_titlebar_bg_inactive{0.15, 0.15, 0.2, 1.0},
    theme_titlebar_fg_active{0.9, 0.9, 0.93, 1.0},
    theme_titlebar_fg_inactive{0.7, 0.7, 0.73, 0.7},
    theme_button_bg{0.4, 0.4, 0.4, 0.3},
    theme_button_hover_bg{0.5, 0.5, 0.5, 0.4},
    theme_button_active_bg{0.3, 0.3, 0.3, 0.5},
    theme_font_family(""),
    theme_font_size(0)
{
}

decoration_theme_t::~decoration_theme_t()
{
    // No cleanup needed for CSS parsing
}

/** Force reload of theme - call when GTK theme/icon theme changes */
void decoration_theme_t::reload_theme() const
{
    LOGI("Reloading GTK theme and icons");
    invalidate_cache();
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

void decoration_theme_t::invalidate_cache() const
{
    bg_cache.valid = false;
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

    // Check if cached surfaces are still valid
    bool cache_hit = bg_cache.valid &&
        bg_cache.geometry.width == rectangle.width &&
        bg_cache.geometry.height == rectangle.height &&
        bg_cache.active == active;

    if (cache_hit)
    {
        // Just re-submit cached textures
        if (bg_cache.titlebar_tex)
        {
            data.pass->add_texture(bg_cache.titlebar_tex->get_texture(),
                data.target, bg_cache.titlebar_rect + wf::point_t{rectangle.x, rectangle.y}
                    - wf::point_t{bg_cache.geometry.x, bg_cache.geometry.y}, data.damage);
        }
        if (bg_cache.left_tex)
        {
            data.pass->add_texture(bg_cache.left_tex->get_texture(),
                data.target, bg_cache.left_rect + wf::point_t{rectangle.x, rectangle.y}
                    - wf::point_t{bg_cache.geometry.x, bg_cache.geometry.y}, data.damage);
        }
        if (bg_cache.right_tex)
        {
            data.pass->add_texture(bg_cache.right_tex->get_texture(),
                data.target, bg_cache.right_rect + wf::point_t{rectangle.x, rectangle.y}
                    - wf::point_t{bg_cache.geometry.x, bg_cache.geometry.y}, data.damage);
        }
        if (bg_cache.bottom_tex)
        {
            data.pass->add_texture(bg_cache.bottom_tex->get_texture(),
                data.target, bg_cache.bottom_rect + wf::point_t{rectangle.x, rectangle.y}
                    - wf::point_t{bg_cache.geometry.x, bg_cache.geometry.y}, data.damage);
        }
        if (bg_cache.outline_tex)
        {
            data.pass->add_texture(bg_cache.outline_tex->get_texture(),
                data.target, bg_cache.outline_rect + wf::point_t{rectangle.x, rectangle.y}
                    - wf::point_t{bg_cache.geometry.x, bg_cache.geometry.y}, data.damage);
        }
        return;
    }

    // Cache miss — regenerate all surfaces
    bg_cache.geometry = rectangle;
    bg_cache.active = active;
    bg_cache.valid = true;

    // Use theme colors if available, otherwise fall back to config colors
    wf::color_t bg_color = active ? theme_titlebar_bg_active : theme_titlebar_bg_inactive;

    // If theme colors are still default (black), use config colors as ultimate fallback
    if (bg_color.r == 0.0 && bg_color.g == 0.0 && bg_color.b == 0.0)
    {
        bg_color = active ? active_color : inactive_color;
    }

    // Use a lighter semi-transparent border to contrast with dark backgrounds (like native GTK)
    wf::color_t outline_color;
    outline_color.r = 1.0;
    outline_color.g = 1.0;
    outline_color.b = 1.0;
    outline_color.a = 0.10;  // Subtle semi-transparent white border

    // Calculate titlebar area (top portion with rounded corners)
    int titlebar_h = title_height + border_size;
    wf::geometry_t titlebar_rect = {
        rectangle.x, rectangle.y,
        rectangle.width, titlebar_h
    };

    // Shadow parameters
    int shadow_offset = 2;
    int shadow_blur = 3;

    // --- Titlebar surface ---
    {
        auto titlebar_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
            titlebar_rect.width + shadow_blur * 2, titlebar_rect.height + shadow_blur);
        auto cr = cairo_create(titlebar_surface);

        cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
        cairo_paint(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

        double radius = corner_radius;
        double x = shadow_blur, y = shadow_blur;
        double w = titlebar_rect.width, h = titlebar_rect.height;

        auto draw_rounded_rect_path = [&]() {
            cairo_new_sub_path(cr);
            cairo_arc(cr, x + radius, y + radius, radius, M_PI, 3 * M_PI / 2);
            cairo_line_to(cr, x + w - radius, y);
            cairo_arc(cr, x + w - radius, y + radius, radius, -M_PI / 2, 0);
            cairo_line_to(cr, x + w, y + h);
            cairo_line_to(cr, x, y + h);
            cairo_close_path(cr);
        };

        // Drop shadow
        for (int i = shadow_blur; i > 0; i--)
        {
            cairo_save(cr);
            cairo_new_sub_path(cr);
            double shadow_x = shadow_blur + 0.5;
            double shadow_y = shadow_blur + shadow_offset + 0.5;
            cairo_arc(cr, shadow_x + radius, shadow_y + radius, radius, M_PI, 3 * M_PI / 2);
            cairo_line_to(cr, shadow_x + w - radius, shadow_y);
            cairo_arc(cr, shadow_x + w - radius, shadow_y + radius, radius, -M_PI / 2, 0);
            cairo_line_to(cr, shadow_x + w, shadow_y + h);
            cairo_line_to(cr, shadow_x, shadow_y + h);
            cairo_close_path(cr);

            double alpha = 0.1 * (1.0 - (double)i / shadow_blur);
            cairo_set_source_rgba(cr, 0, 0, 0, alpha);
            cairo_set_line_width(cr, i * 2.0);
            cairo_stroke(cr);
            cairo_restore(cr);
        }

        draw_rounded_rect_path();
        cairo_set_source_rgba(cr, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
        cairo_fill(cr);
        cairo_destroy(cr);

        bg_cache.titlebar_rect = {
            titlebar_rect.x - shadow_blur,
            titlebar_rect.y - shadow_blur,
            titlebar_rect.width + shadow_blur * 2,
            titlebar_rect.height + shadow_blur
        };
        bg_cache.titlebar_tex = std::make_unique<wf::owned_texture_t>(titlebar_surface);
        data.pass->add_texture(bg_cache.titlebar_tex->get_texture(), data.target,
            bg_cache.titlebar_rect, data.damage);
        cairo_surface_destroy(titlebar_surface);
    }

    // --- Left border ---
    if (border_size > 0)
    {
        int border_h = rectangle.height - corner_radius - bottom_corner_radius + 1;
        auto left_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
            border_size + shadow_blur, border_h);
        auto left_cr = cairo_create(left_surface);

        cairo_set_operator(left_cr, CAIRO_OPERATOR_CLEAR);
        cairo_paint(left_cr);
        cairo_set_operator(left_cr, CAIRO_OPERATOR_OVER);

        for (int i = shadow_blur; i > 0; i--)
        {
            double alpha = 0.1 * (1.0 - (double)i / shadow_blur);
            cairo_set_source_rgba(left_cr, 0, 0, 0, alpha);
            cairo_set_line_width(left_cr, i * 2.0);
            cairo_move_to(left_cr, shadow_blur + 0.5, 0);
            cairo_line_to(left_cr, shadow_blur + 0.5, border_h);
            cairo_stroke(left_cr);
        }

        cairo_set_source_rgba(left_cr, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
        cairo_rectangle(left_cr, shadow_blur, 0, border_size, border_h);
        cairo_fill(left_cr);
        cairo_destroy(left_cr);

        bg_cache.left_rect = {
            rectangle.x - shadow_blur, rectangle.y + corner_radius - 1,
            border_size + shadow_blur, border_h
        };
        bg_cache.left_tex = std::make_unique<wf::owned_texture_t>(left_surface);
        data.pass->add_texture(bg_cache.left_tex->get_texture(), data.target,
            bg_cache.left_rect, data.damage);
        cairo_surface_destroy(left_surface);
    }

    // --- Right border ---
    if (border_size > 0)
    {
        int border_h = rectangle.height - corner_radius - bottom_corner_radius + 1;
        auto right_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
            border_size + shadow_blur, border_h);
        auto right_cr = cairo_create(right_surface);

        cairo_set_operator(right_cr, CAIRO_OPERATOR_CLEAR);
        cairo_paint(right_cr);
        cairo_set_operator(right_cr, CAIRO_OPERATOR_OVER);

        for (int i = shadow_blur; i > 0; i--)
        {
            double alpha = 0.1 * (1.0 - (double)i / shadow_blur);
            cairo_set_source_rgba(right_cr, 0, 0, 0, alpha);
            cairo_set_line_width(right_cr, i * 2.0);
            cairo_move_to(right_cr, border_size - 0.5, 0);
            cairo_line_to(right_cr, border_size - 0.5, border_h);
            cairo_stroke(right_cr);
        }

        cairo_set_source_rgba(right_cr, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
        cairo_rectangle(right_cr, 0, 0, border_size, border_h);
        cairo_fill(right_cr);
        cairo_destroy(right_cr);

        bg_cache.right_rect = {
            rectangle.x + rectangle.width - border_size, rectangle.y + corner_radius - 1,
            border_size + shadow_blur, border_h
        };
        bg_cache.right_tex = std::make_unique<wf::owned_texture_t>(right_surface);
        data.pass->add_texture(bg_cache.right_tex->get_texture(), data.target,
            bg_cache.right_rect, data.damage);
        cairo_surface_destroy(right_surface);
    }

    // --- Bottom border with rounded bottom corners ---
    if (border_size > 0)
    {
        double br = bottom_corner_radius;
        int bottom_h = border_size + (int)br;  // Extra height for corner arcs
        int surface_w = rectangle.width + shadow_blur * 2;
        int surface_h = bottom_h + shadow_blur;
        auto bottom_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
            surface_w, surface_h);
        auto bottom_cr = cairo_create(bottom_surface);

        cairo_set_operator(bottom_cr, CAIRO_OPERATOR_CLEAR);
        cairo_paint(bottom_cr);
        cairo_set_operator(bottom_cr, CAIRO_OPERATOR_OVER);

        // Draw shadow along the bottom with rounded corners
        for (int i = shadow_blur; i > 0; i--)
        {
            double alpha = 0.1 * (1.0 - (double)i / shadow_blur);
            cairo_set_source_rgba(bottom_cr, 0, 0, 0, alpha);
            cairo_set_line_width(bottom_cr, i * 2.0);

            double sx = shadow_blur;
            double sy = shadow_offset;
            double sw = rectangle.width;

            cairo_new_sub_path(bottom_cr);
            cairo_move_to(bottom_cr, sx, sy + 0.5);
            cairo_line_to(bottom_cr, sx + sw, sy + 0.5);
            cairo_line_to(bottom_cr, sx + sw, sy + bottom_h - br + 0.5);
            cairo_arc(bottom_cr, sx + sw - br, sy + bottom_h - br + 0.5, br, 0, M_PI / 2);
            cairo_line_to(bottom_cr, sx + br, sy + bottom_h + 0.5);
            cairo_arc(bottom_cr, sx + br, sy + bottom_h - br + 0.5, br, M_PI / 2, M_PI);
            cairo_close_path(bottom_cr);
            cairo_stroke(bottom_cr);
        }

        // Fill bottom area with rounded bottom corners
        double bx = shadow_blur;
        double by = 0;
        double bw = rectangle.width;

        cairo_new_sub_path(bottom_cr);
        cairo_move_to(bottom_cr, bx, by);
        cairo_line_to(bottom_cr, bx + bw, by);
        cairo_line_to(bottom_cr, bx + bw, by + bottom_h - br);
        cairo_arc(bottom_cr, bx + bw - br, by + bottom_h - br, br, 0, M_PI / 2);
        cairo_line_to(bottom_cr, bx + br, by + bottom_h);
        cairo_arc(bottom_cr, bx + br, by + bottom_h - br, br, M_PI / 2, M_PI);
        cairo_close_path(bottom_cr);

        cairo_set_source_rgba(bottom_cr, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
        cairo_fill(bottom_cr);
        cairo_destroy(bottom_cr);

        bg_cache.bottom_rect = {
            rectangle.x - shadow_blur,
            rectangle.y + rectangle.height - border_size - (int)br,
            surface_w, surface_h
        };
        bg_cache.bottom_tex = std::make_unique<wf::owned_texture_t>(bottom_surface);
        data.pass->add_texture(bg_cache.bottom_tex->get_texture(), data.target,
            bg_cache.bottom_rect, data.damage);
        cairo_surface_destroy(bottom_surface);
    }

    // --- Unified outline: rounded top corners, rounded bottom corners ---
    {
        auto outline_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
            rectangle.width, rectangle.height);
        auto outline_cr = cairo_create(outline_surface);

        cairo_set_operator(outline_cr, CAIRO_OPERATOR_CLEAR);
        cairo_paint(outline_cr);
        cairo_set_operator(outline_cr, CAIRO_OPERATOR_OVER);

        double r = corner_radius;
        double br = bottom_corner_radius;
        double w = rectangle.width;
        double h = rectangle.height;

        cairo_new_sub_path(outline_cr);
        // Top-left arc
        cairo_arc(outline_cr, 0.5 + r, 0.5 + r, r, M_PI, 3 * M_PI / 2);
        // Top-right arc
        cairo_arc(outline_cr, w - 0.5 - r, 0.5 + r, r, -M_PI / 2, 0);
        // Down to bottom-right arc
        cairo_line_to(outline_cr, w - 0.5, h - 0.5 - br);
        cairo_arc(outline_cr, w - 0.5 - br, h - 0.5 - br, br, 0, M_PI / 2);
        // Across bottom to bottom-left arc
        cairo_arc(outline_cr, 0.5 + br, h - 0.5 - br, br, M_PI / 2, M_PI);
        // Close path (up left side)
        cairo_close_path(outline_cr);

        cairo_set_source_rgba(outline_cr, outline_color.r, outline_color.g,
            outline_color.b, outline_color.a);
        cairo_set_line_width(outline_cr, 1.0);
        cairo_stroke(outline_cr);
        cairo_destroy(outline_cr);

        bg_cache.outline_rect = {
            rectangle.x, rectangle.y,
            rectangle.width, rectangle.height
        };
        bg_cache.outline_tex = std::make_unique<wf::owned_texture_t>(outline_surface);
        data.pass->add_texture(bg_cache.outline_tex->get_texture(), data.target,
            bg_cache.outline_rect, data.damage);
        cairo_surface_destroy(outline_surface);
    }
}

/**
 * Render the given text on a cairo_surface_t with the given size.
 * The caller is responsible for freeing the memory afterwards.
 */
cairo_surface_t*decoration_theme_t::render_text(const std::string& text,
    int width, int height, int button_area_width) const
{
    const auto format = CAIRO_FORMAT_ARGB32;
    auto surface = cairo_image_surface_create(format, width, height);

    if (height == 0)
    {
        return surface;
    }

    // Lazy initialization: load GTK theme to get font settings
    load_gtk_theme();

    wf::color_t color = theme_titlebar_fg_active;
    // Use config font color as fallback
    if (color.r == 0.0 && color.g == 0.0 && color.b == 0.0)
    {
        color = font_color;
    }

    auto cr = cairo_create(surface);

    PangoFontDescription *font_desc;
    PangoLayout *layout;

    // Use GTK font with scaling to match native GTK titlebar size
    std::string gtk_font_now = get_gtk_font_name();
    std::string font_to_use;

    if (!gtk_font_now.empty())
    {
        // Parse the GTK font (format: "Font Name Weight Size")
        size_t last_space = gtk_font_now.rfind(' ');
        if (last_space != std::string::npos)
        {
            std::string font_family = gtk_font_now.substr(0, last_space);
            std::string font_size_str = gtk_font_now.substr(last_space + 1);
            try {
                int base_size = std::stoi(font_size_str);
                // Scale font size to match native GTK titlebar (typically 1.3-1.4x larger)
                int scaled_size = static_cast<int>(base_size * 1.12);
                font_to_use = font_family + " Bold " + std::to_string(scaled_size);
            } catch (...) {
                font_to_use = gtk_font_now;  // Use as-is if parsing fails
            }
        }
        else
        {
            font_to_use = gtk_font_now + " 11";
        }
    }
    else
    {
        // Fallback if GTK font can't be read
        font_to_use = "Sans 11";
    }

    font_desc = pango_font_description_from_string(font_to_use.c_str());

    layout = pango_cairo_create_layout(cr);
    pango_layout_set_font_description(layout, font_desc);
    pango_layout_set_text(layout, text.c_str(), text.size());

    // Reserve space for buttons on the left and mirror on the right for centering
    int left_padding = (button_area_width > 0) ? button_area_width : 10;
    int right_padding = left_padding;  // Mirror for true visual centering
    int text_width = width - left_padding - right_padding;
    if (text_width < 0) text_width = width;  // Fallback if window too narrow
    int text_x = left_padding;

    // Center the text horizontally within the available space
    pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
    pango_layout_set_width(layout, text_width * PANGO_SCALE);

    // Prevent wrapping and add ellipsis for long titles
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
    pango_layout_set_single_paragraph_mode(layout, TRUE);

    // Center vertically
    PangoRectangle ink_rect, logical_rect;
    pango_layout_get_pixel_extents(layout, &ink_rect, &logical_rect);
    int text_y = (height - logical_rect.height) / 2 - logical_rect.y;

    cairo_move_to(cr, text_x, text_y);
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

    // Draw icon when window is active or when button is hovered (like native GTK)
    if (state.activated || state.hover_progress > 0)
    {
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
                double icon_size = state.width * 0.85;  // Match GTK size with visible padding
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
        // cppcheck-suppress knownConditionTrueFalse
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
    } // end if (state.activated)

    cairo_destroy(cr);
    return button_surface;
}
}
}
