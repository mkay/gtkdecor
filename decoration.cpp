#include <wayfire/per-output-plugin.hpp>
#include <wayfire/view.hpp>
#include <wayfire/workarea.hpp>
#include <wayfire/matcher.hpp>
#include <wayfire/workspace-set.hpp>
#include <wayfire/output.hpp>
#include <wayfire/signal-definitions.hpp>
#include <wayfire/txn/transaction-manager.hpp>

#include "deco-subsurface.hpp"
#include "wayfire/core.hpp"
#include "wayfire/plugin.hpp"
#include "wayfire/signal-provider.hpp"
#include "wayfire/toplevel-view.hpp"
#include "wayfire/toplevel.hpp"

#include <sys/inotify.h>
#include <unistd.h>
#include <fcntl.h>

class wayfire_gtkdecor : public wf::plugin_interface_t
{
    wf::view_matcher_t ignore_views{"gtkdecor/ignore_views"};
    wf::view_matcher_t forced_views{"gtkdecor/forced_views"};

    // GTK settings file monitoring
    int inotify_fd = -1;
    int watch_fd = -1;
    wl_event_source *inotify_source = nullptr;

    wf::signal::connection_t<wf::txn::new_transaction_signal> on_new_tx =
        [=] (wf::txn::new_transaction_signal *ev)
    {
        // For each transaction, we need to consider what happens with participating views
        for (const auto& obj : ev->tx->get_objects())
        {
            if (auto toplevel = std::dynamic_pointer_cast<wf::toplevel_t>(obj))
            {
                // First check whether the toplevel already has decoration
                // In that case, we should just set the correct margins
                if (auto deco = toplevel->get_data<wf::simple_decorator_t>())
                {
                    toplevel->pending().margins = deco->get_margins(toplevel->pending());
                    continue;
                }

                // Second case: the view is already mapped, or the transaction does not map it.
                // The view is not being decorated, so nothing to do here.
                if (toplevel->current().mapped || !toplevel->pending().mapped)
                {
                    continue;
                }

                // Third case: the transaction will map the toplevel.
                auto view = wf::find_view_for_toplevel(toplevel);
                wf::dassert(view != nullptr, "Mapping a toplevel means there must be a corresponding view!");
                if (should_decorate_view(view))
                {
                    adjust_new_decorations(view);
                }
            }
        }
    };

    wf::signal::connection_t<wf::view_decoration_state_updated_signal> on_decoration_state_changed =
        [=] (wf::view_decoration_state_updated_signal *ev)
    {
        update_view_decoration(ev->view);
    };

    // allows criteria containing maximized or floating check
    wf::signal::connection_t<wf::view_tiled_signal> on_view_tiled =
        [=] (wf::view_tiled_signal *ev)
    {
        update_view_decoration(ev->view);
    };

    static int handle_inotify_event(int fd, uint32_t mask, void *data)
    {
        wayfire_gtkdecor *self = static_cast<wayfire_gtkdecor*>(data);

        char buffer[1024];
        int length = read(fd, buffer, sizeof(buffer));
        if (length < 0)
        {
            return 0;
        }

        // Parse inotify events
        int i = 0;
        while (i < length)
        {
            struct inotify_event *event = (struct inotify_event*)&buffer[i];
            if (event->mask & (IN_MODIFY | IN_CLOSE_WRITE))
            {
                LOGI("GTK settings changed, reloading decorations");
                self->reload_all_decorations();
            }
            i += sizeof(struct inotify_event) + event->len;
        }

        return 0;
    }

    void setup_gtk_settings_monitor()
    {
        const char *home = getenv("HOME");
        if (!home)
        {
            return;
        }

        std::string settings_path = std::string(home) + "/.config/gtk-3.0/settings.ini";

        inotify_fd = inotify_init1(IN_NONBLOCK);
        if (inotify_fd < 0)
        {
            LOGE("Failed to initialize inotify for GTK settings monitoring");
            return;
        }

        watch_fd = inotify_add_watch(inotify_fd, settings_path.c_str(), IN_MODIFY | IN_CLOSE_WRITE);
        if (watch_fd < 0)
        {
            LOGE("Failed to watch GTK settings file: ", settings_path);
            close(inotify_fd);
            inotify_fd = -1;
            return;
        }

        auto display = wf::get_core().display;
        auto event_loop = wl_display_get_event_loop(display);
        inotify_source = wl_event_loop_add_fd(event_loop, inotify_fd, WL_EVENT_READABLE,
                                              handle_inotify_event, this);

        LOGI("Monitoring GTK settings file for theme changes: ", settings_path);
    }

    void cleanup_gtk_settings_monitor()
    {
        if (inotify_source)
        {
            wl_event_source_remove(inotify_source);
            inotify_source = nullptr;
        }

        if (watch_fd >= 0)
        {
            inotify_rm_watch(inotify_fd, watch_fd);
            watch_fd = -1;
        }

        if (inotify_fd >= 0)
        {
            close(inotify_fd);
            inotify_fd = -1;
        }
    }

    void reload_all_decorations()
    {
        for (auto& view : wf::get_core().get_all_views())
        {
            if (auto toplevel = wf::toplevel_cast(view))
            {
                if (auto deco = toplevel->toplevel()->get_data<wf::simple_decorator_t>())
                {
                    deco->reload_theme();
                    view->damage();
                }
            }
        }
    }

  public:
    void init() override
    {
        LOGE("=== DECORATION PLUGIN INIT() CALLED ===");
        wf::get_core().connect(&on_decoration_state_changed);
        wf::get_core().tx_manager->connect(&on_new_tx);
        wf::get_core().connect(&on_view_tiled);

        for (auto& view : wf::get_core().get_all_views())
        {
            update_view_decoration(view);
        }

        setup_gtk_settings_monitor();
    }

    void fini() override
    {
        cleanup_gtk_settings_monitor();

        for (auto view : wf::get_core().get_all_views())
        {
            if (auto toplevel = wf::toplevel_cast(view))
            {
                remove_decoration(toplevel);
                wf::get_core().tx_manager->schedule_object(toplevel->toplevel());
            }
        }
    }

    /**
     * Uses view_matcher_t to match whether the given view needs to be
     * ignored for decoration
     *
     * @param view The view to match
     * @return Whether the given view should be decorated?
     */
    bool ignore_decoration_of_view(wayfire_view view)
    {
        return ignore_views.matches(view);
    }

    /**
     * Uses view_matcher_t to match whether to force decorations onto the
     * given view
     *
     * @param view The view to match
     * @return Whether the given view should be decorated?
     */
    bool force_decoration_of_view(wayfire_view view)
    {
        return forced_views.matches(view);
    }

    bool should_decorate_view(wayfire_toplevel_view view)
    {
        return !ignore_decoration_of_view(view) &&
               (force_decoration_of_view(view) || view->should_be_decorated());
    }

    void adjust_new_decorations(wayfire_toplevel_view view)
    {
        LOGI("Adding decoration to view: ", view->get_title());
        auto toplevel = view->toplevel();

        toplevel->store_data(std::make_unique<wf::simple_decorator_t>(view));
        auto deco     = toplevel->get_data<wf::simple_decorator_t>();
        auto& pending = toplevel->pending();
        pending.margins = deco->get_margins(pending);

        if (!pending.fullscreen && !pending.tiled_edges)
        {
            pending.geometry = wf::expand_geometry_by_margins(pending.geometry, pending.margins);
            if (view->get_output())
            {
                pending.geometry = wf::clamp(pending.geometry, view->get_output()->workarea->get_workarea());
            }
        }
    }

    void remove_decoration(wayfire_toplevel_view view)
    {
        view->toplevel()->erase_data<wf::simple_decorator_t>();
        auto& pending = view->toplevel()->pending();
        if (!pending.fullscreen && !pending.tiled_edges)
        {
            pending.geometry = wf::shrink_geometry_by_margins(pending.geometry, pending.margins);
        }

        pending.margins = {0, 0, 0, 0};
    }

    bool is_toplevel_decorated(const std::shared_ptr<wf::toplevel_t>& toplevel)
    {
        return toplevel->has_data<wf::simple_decorator_t>();
    }

    void update_view_decoration(wayfire_view view)
    {
        if (auto toplevel = wf::toplevel_cast(view))
        {
            const bool wants_decoration = should_decorate_view(toplevel);
            if (wants_decoration != is_toplevel_decorated(toplevel->toplevel()))
            {
                if (wants_decoration)
                {
                    adjust_new_decorations(toplevel);
                } else
                {
                    remove_decoration(toplevel);
                }

                wf::get_core().tx_manager->schedule_object(toplevel->toplevel());
            }
        }
    }
};

DECLARE_WAYFIRE_PLUGIN(wayfire_gtkdecor);
