#include <furi.h>

#include <gui/gui.h>
#include <gui/icon_i.h>

/* generated by fbt from .png files in images folder */
#include <mytestapp_icons.h>

#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64

#define FPS 30

typedef enum {
    MyEventTypeKey,
    MyEventTypeDone,
} MyEventType;

typedef struct {
    MyEventType type; // The reason for this event.
    InputEvent input; // This data is specific to keypress data.
} MyEvent;

typedef struct {
    // Player coordinates
    short int playerX;
    // Direction of player movement
    short int playerDirection;

    // Projectile
    bool shoot;
    short int projectileX;
    short int projectileY;

    // Enemies
    int enemyX[8];
    int enemyY;
    short int enemyDirection;
    short int enemyCount;

    // Time of run (ms)
    int time;
} TestAppState;

typedef struct {
    FuriMessageQueue* queue; // Event queue
    FuriTimer* timer; // Timer for redrawing the screen
    ViewPort* view_port; // Input and draw callbacks
    Gui* gui; // Fullscreen view

    Icon* playerIcon; // Player ship
    Icon* enemyIcon; // Enemy ship

    TestAppState state; // Application data
} TestApp;

void init_game_state(TestApp* app) {
    app->state.playerX = (DISPLAY_WIDTH - icon_get_width(app->playerIcon)) / 2;
    app->state.playerDirection = 0;
    app->state.time = 0;
    app->state.shoot = false;
    app->state.enemyDirection = 1;

    for(short int i = 0; i < 8; i++) {
        app->state.enemyX[i] = i * 12;
    }
    app->state.enemyY = 0;
    app->state.enemyCount = 8;
}

static void my_draw_callback(Canvas* canvas, void* context) {
    furi_assert(context);
    TestApp* app = (TestApp*)context;

    canvas_draw_bitmap(canvas, app->state.playerX, 56, 13, 8, icon_get_data(app->playerIcon));

    for(short int i = 0; i < app->state.enemyCount; i++) {
        canvas_draw_bitmap(
            canvas, app->state.enemyX[i], app->state.enemyY, 8, 8, icon_get_data(app->enemyIcon));
    }

    if(app->state.shoot) {
        canvas_draw_line(
            canvas,
            app->state.projectileX,
            app->state.projectileY < 0 ? 0 : app->state.projectileY,
            app->state.projectileX,
            app->state.projectileY + 3);
    }
}

static void my_input_callback(InputEvent* input_event, void* context) {
    furi_assert(context);
    TestApp* app = (TestApp*)context;

    if(input_event->type == InputTypeShort) {
        if(input_event->key == InputKeyBack) {
            MyEvent event;
            event.type = MyEventTypeDone;
            furi_message_queue_put(app->queue, &event, FuriWaitForever);
        } else if(input_event->key == InputKeyOk && !app->state.shoot) {
            app->state.projectileX = app->state.playerX + 6;
            app->state.projectileY = DISPLAY_HEIGHT - 8;
            app->state.shoot = true;
        }
    } else if(input_event->type == InputTypePress) {
        if(input_event->key == InputKeyLeft)
            app->state.playerDirection = -1;
        else if(input_event->key == InputKeyRight)
            app->state.playerDirection = 1;
    } else if(input_event->type == InputTypeRelease) {
        if(input_event->key == InputKeyLeft || input_event->key == InputKeyRight) {
            app->state.playerDirection = 0;
        }
    }
}

static void timer_callback(void* context) {
    furi_assert(context);
    TestApp* app = (TestApp*)context;

    // Player movement
    app->state.playerX = app->state.playerX + app->state.playerDirection;
    if(app->state.playerX > DISPLAY_WIDTH - 13) app->state.playerX = DISPLAY_WIDTH;
    if(app->state.playerX < 0) app->state.playerX = 0;

    // Projectile movement
    if(app->state.shoot) {
        app->state.projectileY--;
        if(app->state.projectileY < -3) {
            app->state.shoot = false;
        }
    }

    // Enemy movement
    if(app->state.time % 2) {
        if(app->state.enemyDirection == 1 &&
           app->state.enemyX[app->state.enemyCount - 1] + 8 >= DISPLAY_WIDTH) {
            app->state.enemyDirection = -1;
            app->state.enemyY++;
        } else if(app->state.enemyDirection == -1 && app->state.enemyX[0] <= 0) {
            app->state.enemyDirection = 1;
            app->state.enemyY++;
        } else {
            for(short int i = 0; i < app->state.enemyCount; i++) {
                app->state.enemyX[i] += app->state.enemyDirection;
            }
        }
    }

    if(app->state.shoot) {
        if(app->state.projectileY < app->state.enemyY + 8 &&
           app->state.projectileY > app->state.enemyY) {
            for(short int j = 0; j < app->state.enemyCount; j++) {
                if(app->state.projectileX > app->state.enemyX[j] &&
                   app->state.projectileX < app->state.enemyX[j] + 8) {
                    if(app->state.enemyCount == 1) {
                        // Win
                        init_game_state(app);
                        return;
                    }
                    for(short int k = j; k <= app->state.enemyCount - 1; k++) {
                        app->state.enemyX[k] = app->state.enemyX[k + 1];
                    }
                    app->state.enemyCount--;
                    app->state.shoot = false;
                    break;
                }
            }
        }
    }

    app->state.time += 1;
}

int32_t mytestapp_app(void* p) {
    UNUSED(p);

    // ---------------
    //      Init
    // ---------------
    TestApp* app = (TestApp*)malloc(sizeof(TestApp));

    // Create event queue
    app->queue = furi_message_queue_alloc(8, sizeof(MyEvent));

    // Create view port
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, my_draw_callback, app);
    view_port_input_callback_set(app->view_port, my_input_callback, app);
    view_port_set_orientation(app->view_port, ViewPortOrientationHorizontal);

    // Create gui
    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    // Create timer
    app->timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, 1000 / FPS);

    // ---------------
    //      Setup
    // ---------------

    app->playerIcon = (Icon*)&I_player;
    app->enemyIcon = (Icon*)&I_enemy;

    init_game_state(app);

    // ---------------
    //    Main loop
    // ---------------
    MyEvent event;
    while(true) {
        if(furi_message_queue_get(app->queue, &event, FuriWaitForever) == FuriStatusOk) {
            if(event.type == MyEventTypeDone) {
                break;
            }
        } else {
            break;
        }
    }

    // ---------------
    //     Cleanup
    // ---------------

    // Free resources
    furi_timer_free(app->timer);
    furi_message_queue_free(app->queue);
    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    free(app);

    return 0;
}
