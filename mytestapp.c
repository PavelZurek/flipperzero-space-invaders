#include <furi.h>

#include <gui/gui.h>
#include <gui/icon_i.h>

/* generated by fbt from .png files in images folder */
#include <space_invaders_icons.h>

#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64

#define FPS 30

#define MAX_EXPLOSIONS 3
#define EXPLOSION_DURATION 15 // In frames

typedef enum {
    MyEventTypeKey,
    MyEventTypeDone,
} MyEventType;

typedef struct {
    MyEventType type; // The reason for this event.
    InputEvent input; // This data is specific to keypress data.
} MyEvent;

typedef struct {
    int x;
    int y;
    int time;
} Explosion;

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
    int enemyX[3][8];
    int enemyY[3];
    short int enemyDirection;
    short int enemyCount[3];

    // Expolosions
    int explosionCount;
    Explosion explosion[MAX_EXPLOSIONS];

    // Time of run (ms)
    int time;
} TestAppState;

typedef struct {
    FuriMessageQueue* queue; // Event queue
    FuriTimer* timer; // Timer for redrawing the screen
    ViewPort* view_port; // Input and draw callbacks
    Gui* gui; // Fullscreen view

    Icon* playerIcon; // Player ship
    Icon* enemyIcon[3]; // Enemy ships
    Icon* boomIcon; // Explosion

    TestAppState state; // Application data
} TestApp;

void init_game_state(TestApp* app) {
    app->state.playerX = (DISPLAY_WIDTH - 13) / 2;
    app->state.playerDirection = 0;
    app->state.time = 0;
    app->state.shoot = false;
    app->state.enemyDirection = 1;
    app->state.explosionCount = 0;

    for(short int et = 0; et < 3; et++) {
        for(short int i = 0; i < 8; i++) {
            app->state.enemyX[et][i] = i * 15;
            if(et == 0) app->state.enemyX[et][i] += 2;
        }
        app->state.enemyY[et] = et * 12;
        app->state.enemyCount[et] = 8;
    }
}

static void my_draw_callback(Canvas* canvas, void* context) {
    furi_assert(context);
    TestApp* app = (TestApp*)context;

    for(short int i = 0; i < app->state.explosionCount; i++) {
        canvas_draw_bitmap(
            canvas,
            app->state.explosion[i].x,
            app->state.explosion[i].y,
            11,
            9,
            icon_get_data(app->boomIcon));
    }

    canvas_draw_bitmap(canvas, app->state.playerX, 56, 13, 8, icon_get_data(app->playerIcon));

    for(short int et = 0; et < 3; et++) {
        for(short int i = 0; i < app->state.enemyCount[et]; i++) {
            canvas_draw_bitmap(
                canvas,
                app->state.enemyX[et][i],
                app->state.enemyY[et],
                icon_get_width(app->enemyIcon[et]),
                icon_get_height(app->enemyIcon[et]),
                icon_get_data(app->enemyIcon[et]));
        }
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
    if(app->state.playerX > DISPLAY_WIDTH - 13) app->state.playerX = DISPLAY_WIDTH - 13;
    if(app->state.playerX < 0) app->state.playerX = 0;

    // Projectile movement
    if(app->state.shoot) {
        app->state.projectileY--;
        if(app->state.projectileY < -3) {
            app->state.shoot = false;
        }
    }

    // Enemy movement
    if(app->state.time % 3 == 0) {
        int enemyDirection = app->state.enemyDirection;
        int maxEnemyX = 0;
        int minEnemyX = DISPLAY_WIDTH;

        for(short int et = 0; et < 3; et++) {
            if(app->state.enemyCount[et] > 0) {
                int newMaxX = app->state.enemyX[et][app->state.enemyCount[et] - 1] +
                              icon_get_width(app->enemyIcon[et]);
                if(maxEnemyX < newMaxX) maxEnemyX = newMaxX;
                if(minEnemyX > app->state.enemyX[et][0]) minEnemyX = app->state.enemyX[et][0];
            }
        }

        if(enemyDirection == 1 && maxEnemyX >= DISPLAY_WIDTH) {
            app->state.enemyDirection = -1;
            for(short int et = 0; et < 3; et++) {
                app->state.enemyY[et]++;
            }
        } else if(enemyDirection == -1 && minEnemyX <= 0) {
            app->state.enemyDirection = 1;
            for(short int et = 0; et < 3; et++) {
                app->state.enemyY[et]++;
            }
        } else {
            for(short int et = 0; et < 3; et++) {
                for(short int i = 0; i < app->state.enemyCount[et]; i++) {
                    app->state.enemyX[et][i] += app->state.enemyDirection;
                }
            }
        }
    }

    // Explosion duration
    if(app->state.explosionCount < MAX_EXPLOSIONS) {
        for(short int i = app->state.explosionCount - 1; i >= 0; i--) {
            if(app->state.time - app->state.explosion[i].time > EXPLOSION_DURATION) {
                app->state.explosionCount--;
                for(short int j = i; j < app->state.explosionCount; j++) {
                    app->state.explosion[j] = app->state.explosion[j + 1];
                }
            }
        }
    }

    // Test hit
    if(app->state.shoot) {
        for(short int et = 0; et < 3; et++) {
            if(app->state.projectileY < app->state.enemyY[et] + 8 &&
               app->state.projectileY > app->state.enemyY[et] - 3) {
                for(short int j = 0; j < app->state.enemyCount[et]; j++) {
                    if(app->state.projectileX > app->state.enemyX[et][j] &&
                       app->state.projectileX <
                           app->state.enemyX[et][j] + icon_get_width(app->enemyIcon[et])) {
                        Explosion explosion;
                        explosion.x = app->state.enemyX[et][j];
                        explosion.y = app->state.enemyY[et];
                        explosion.time = app->state.time;
                        app->state.explosion[app->state.explosionCount] = explosion;
                        app->state.explosionCount++;
                        for(short int k = j; k <= app->state.enemyCount[et] - 1; k++) {
                            app->state.enemyX[et][k] = app->state.enemyX[et][k + 1];
                        }
                        app->state.enemyCount[et]--;
                        app->state.shoot = false;
                        break;
                    }
                }
            }
            if(!app->state.shoot) break;
        }
        if(app->state.enemyCount[0] == 0 && app->state.enemyCount[1] == 0 &&
           app->state.enemyCount[2] == 0) {
            // Win
            init_game_state(app);
            return;
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
    app->enemyIcon[0] = (Icon*)&I_enemy1;
    app->enemyIcon[1] = (Icon*)&I_enemy2;
    app->enemyIcon[2] = (Icon*)&I_enemy3;
    app->boomIcon = (Icon*)&I_boom;

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
