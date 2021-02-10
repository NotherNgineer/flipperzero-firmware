#include "dolphin_i.h"

bool dolphin_view_first_start_input(InputEvent* event, void* context) {
    furi_assert(event);
    furi_assert(context);
    Dolphin* dolphin = context;
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyLeft) {
            with_view_model(
                dolphin->idle_view_first_start, (DolphinViewFirstStartModel * model) {
                    if(model->page > 0) model->page--;
                    return true;
                });
        } else if(event->key == InputKeyRight) {
            uint32_t page;
            with_view_model(
                dolphin->idle_view_first_start, (DolphinViewFirstStartModel * model) {
                    page = ++model->page;
                    return true;
                });
            if(page > 8) {
                dolphin_save(dolphin);
                view_dispatcher_switch_to_view(dolphin->idle_view_dispatcher, DolphinViewIdleMain);
            }
        }
    }
    // All events consumed
    return true;
}

bool dolphin_view_idle_main_input(InputEvent* event, void* context) {
    furi_assert(event);
    furi_assert(context);
    Dolphin* dolphin = context;

    if(event->type == InputTypeShort) {
        if(event->key == InputKeyOk) {
            with_value_mutex(
                dolphin->menu_vm, (Menu * menu) { menu_ok(menu); });
        } else if(event->key == InputKeyUp) {
            view_dispatcher_switch_to_view(dolphin->idle_view_dispatcher, DolphinViewIdleStats);
        } else if(event->key == InputKeyDown) {
            view_dispatcher_switch_to_view(dolphin->idle_view_dispatcher, DolphinViewIdleDebug);
        }
    }
    // All events consumed
    return true;
}

bool dolphin_view_idle_stats_input(InputEvent* event, void* context) {
    furi_assert(event);
    furi_assert(context);
    Dolphin* dolphin = context;

    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyLeft) {
        dolphin_deed(dolphin, DolphinDeedWrong);
    } else if(event->key == InputKeyRight) {
        dolphin_deed(dolphin, DolphinDeedIButtonRead);
    } else if(event->key == InputKeyOk) {
        dolphin_save(dolphin);
    } else {
        return false;
    }

    return true;
}

Dolphin* dolphin_alloc() {
    Dolphin* dolphin = furi_alloc(sizeof(Dolphin));
    // Message queue
    dolphin->event_queue = osMessageQueueNew(8, sizeof(DolphinEvent), NULL);
    furi_check(dolphin->event_queue);
    // State
    dolphin->state = dolphin_state_alloc();
    // Menu
    dolphin->menu_vm = furi_record_open("menu");
    // GUI
    dolphin->idle_view_dispatcher = view_dispatcher_alloc();
    // First start View
    dolphin->idle_view_first_start = view_alloc();
    view_allocate_model(
        dolphin->idle_view_first_start, ViewModelTypeLockFree, sizeof(DolphinViewFirstStartModel));
    view_set_context(dolphin->idle_view_first_start, dolphin);
    view_set_draw_callback(dolphin->idle_view_first_start, dolphin_view_first_start_draw);
    view_set_input_callback(dolphin->idle_view_first_start, dolphin_view_first_start_input);
    view_dispatcher_add_view(
        dolphin->idle_view_dispatcher, DolphinViewFirstStart, dolphin->idle_view_first_start);
    // Main Idle View
    dolphin->idle_view_main = view_alloc();
    view_set_context(dolphin->idle_view_main, dolphin);
    view_set_draw_callback(dolphin->idle_view_main, dolphin_view_idle_main_draw);
    view_set_input_callback(dolphin->idle_view_main, dolphin_view_idle_main_input);
    view_dispatcher_add_view(
        dolphin->idle_view_dispatcher, DolphinViewIdleMain, dolphin->idle_view_main);
    // Stats Idle View
    dolphin->idle_view_stats = view_alloc();
    view_set_context(dolphin->idle_view_stats, dolphin);
    view_allocate_model(
        dolphin->idle_view_stats, ViewModelTypeLockFree, sizeof(DolphinViewIdleStatsModel));
    view_set_draw_callback(dolphin->idle_view_stats, dolphin_view_idle_stats_draw);
    view_set_input_callback(dolphin->idle_view_stats, dolphin_view_idle_stats_input);
    view_set_previous_callback(dolphin->idle_view_stats, dolphin_view_idle_back);
    view_dispatcher_add_view(
        dolphin->idle_view_dispatcher, DolphinViewIdleStats, dolphin->idle_view_stats);
    // Debug Idle View
    dolphin->idle_view_debug = view_alloc();
    view_set_draw_callback(dolphin->idle_view_debug, dolphin_view_idle_debug_draw);
    view_set_previous_callback(dolphin->idle_view_debug, dolphin_view_idle_back);
    view_dispatcher_add_view(
        dolphin->idle_view_dispatcher, DolphinViewIdleDebug, dolphin->idle_view_debug);

    return dolphin;
}

void dolphin_save(Dolphin* dolphin) {
    furi_assert(dolphin);
    DolphinEvent event;
    event.type = DolphinEventTypeSave;
    furi_check(osMessageQueuePut(dolphin->event_queue, &event, 0, osWaitForever) == osOK);
}

void dolphin_deed(Dolphin* dolphin, DolphinDeed deed) {
    furi_assert(dolphin);
    DolphinEvent event;
    event.type = DolphinEventTypeDeed;
    event.deed = deed;
    furi_check(osMessageQueuePut(dolphin->event_queue, &event, 0, osWaitForever) == osOK);
}

void dolphin_task() {
    Dolphin* dolphin = dolphin_alloc();

    Gui* gui = furi_record_open("gui");
    view_dispatcher_attach_to_gui(dolphin->idle_view_dispatcher, gui, ViewDispatcherTypeWindow);
    if(dolphin_state_load(dolphin->state)) {
        view_dispatcher_switch_to_view(dolphin->idle_view_dispatcher, DolphinViewIdleMain);
    } else {
        view_dispatcher_switch_to_view(dolphin->idle_view_dispatcher, DolphinViewFirstStart);
    }
    with_view_model(
        dolphin->idle_view_stats, (DolphinViewIdleStatsModel * model) {
            model->icounter = dolphin_state_get_icounter(dolphin->state);
            model->butthurt = dolphin_state_get_butthurt(dolphin->state);
            return true;
        });

    furi_record_create("dolphin", dolphin);

    DolphinEvent event;
    while(1) {
        furi_check(osMessageQueueGet(dolphin->event_queue, &event, NULL, osWaitForever) == osOK);
        if(event.type == DolphinEventTypeDeed) {
            dolphin_state_on_deed(dolphin->state, event.deed);
            with_view_model(
                dolphin->idle_view_stats, (DolphinViewIdleStatsModel * model) {
                    model->icounter = dolphin_state_get_icounter(dolphin->state);
                    model->butthurt = dolphin_state_get_butthurt(dolphin->state);
                    return true;
                });
        } else if(event.type == DolphinEventTypeSave) {
            dolphin_state_save(dolphin->state);
        }
    }
}