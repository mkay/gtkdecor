#pragma once
#include <wayfire/render-manager.hpp>
#include <wayfire/scene-render.hpp>
#include "deco-button.hpp"
#include <string>
#include <map>

namespace wf
{
namespace decor
{
/**
 * A  class which manages the outlook of decorations.
 * It is responsible for determining the background colors, sizes, etc.
 */
class decoration_theme_t
{
  public:
    /** Create a new theme with the default parameters */
    decoration_theme_t();
    ~decoration_theme_t();

    /** @return The available height for displaying the title */
    int get_title_height() const;
    /** @return The available border for resizing */
    int get_border_size() const;
    /** Set the flags for buttons */
    void set_buttons(button_type_t flags);
    button_type_t button_flags;

    /**
     * Fill the given rectangle with the background color(s).
     *
     * @param data The render data (pass, target, damage)
     * @param rectangle The rectangle to redraw.
     * @param active Whether to use active or inactive colors
     */
    void render_background(const wf::scene::render_instruction_t& data,
        wf::geometry_t rectangle, bool active) const;

    /**
     * Get corner radius for rounded titlebar
     */
    int get_corner_radius() const;

    /**
     * Render the given text on a cairo_surface_t with the given size.
     * The caller is responsible for freeing the memory afterwards.
     */
    cairo_surface_t *render_text(std::string text, int width, int height) const;

    struct button_state_t
    {
        /** Button width */
        double width;
        /** Button height */
        double height;
        /** Button outline size */
        double border;
        /** Progress of button hover, in range [-1, 1].
         * Negative numbers are usually used for pressed state. */
        double hover_progress;
    };

    /**
     * Get the icon for the given button.
     * The caller is responsible for freeing the memory afterwards.
     *
     * @param button The button type.
     * @param state The button state.
     */
    cairo_surface_t *get_button_surface(button_type_t button,
        const button_state_t& state) const;

    // Force reload of theme (called when theme changes)
    void reload_theme() const;

  private:
    wf::option_wrapper_t<std::string> font{"gtkdecor/font"};
    wf::option_wrapper_t<wf::color_t> font_color{"gtkdecor/font_color"};
    wf::option_wrapper_t<int> title_height{"gtkdecor/title_height"};
    wf::option_wrapper_t<int> border_size{"gtkdecor/border_size"};
    wf::option_wrapper_t<wf::color_t> active_color{"gtkdecor/active_color"};
    wf::option_wrapper_t<wf::color_t> inactive_color{"gtkdecor/inactive_color"};

    // Rounded corner radius for titlebar
    const int corner_radius = 12;

    // GTK theme parsing (mutable for lazy initialization)
    mutable bool theme_loaded;
    mutable wf::color_t theme_titlebar_bg_active;
    mutable wf::color_t theme_titlebar_bg_inactive;
    mutable wf::color_t theme_titlebar_fg_active;
    mutable wf::color_t theme_titlebar_fg_inactive;
    mutable wf::color_t theme_button_bg;
    mutable wf::color_t theme_button_hover_bg;
    mutable wf::color_t theme_button_active_bg;
    mutable std::string theme_font_family;
    mutable int theme_font_size;
    mutable std::string icon_theme_name;

    void load_gtk_theme() const;
    std::string get_gtk_theme_name() const;
    std::string get_icon_theme_name() const;
    std::string get_gtk_font_name() const;
    std::string find_theme_css_file(const std::string& theme_name) const;
    std::string find_icon_file(const std::string& icon_name, int size) const;
    void parse_theme_css(const std::string& css_file) const;
    wf::color_t parse_css_color(const std::string& color_str) const;
};
}
}
