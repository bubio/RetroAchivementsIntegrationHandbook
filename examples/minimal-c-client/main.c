#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * This is a standalone host-side lifecycle sketch. It does not link rcheevos;
 * replace the fake_ra_* functions with rc_client calls in a real emulator.
 */

typedef enum {
  RA_STATUS_LOGGED_OUT,
  RA_STATUS_AUTHENTICATING,
  RA_STATUS_READY,
  RA_STATUS_LOADING_GAME,
  RA_STATUS_ACTIVE,
  RA_STATUS_OFFLINE_SESSION
} ra_status;

typedef struct {
  ra_status status;
  uint64_t generation;
  int hardcore_enabled;
  int dirty_achievements;
  int dirty_leaderboards;
  int dirty_user;
  int pending_ui_notifications;
  int frame_count;
} ra_runtime;

typedef enum {
  RA_ACTION_PAUSE,
  RA_ACTION_LOAD_STATE,
  RA_ACTION_SAVE_STATE,
  RA_ACTION_FULL_SPEED
} ra_action;

static uint32_t read_inspection_memory(uint32_t address, uint8_t* buffer,
                                       uint32_t count) {
  if (!buffer || address >= 0x10000)
    return 0;

  if (count > 0x10000 - address)
    count = 0x10000 - address;

  memset(buffer, 0, count);
  return count;
}

static void fake_ra_do_frame(ra_runtime* runtime) {
  uint8_t bytes[4];
  read_inspection_memory(0x1234, bytes, sizeof(bytes));

  runtime->frame_count++;
  if (runtime->frame_count == 3) {
    runtime->dirty_achievements = 1;
    runtime->dirty_user = 1;
    runtime->pending_ui_notifications++;
    puts("event: achievement triggered");
  }
}

static void fake_ra_idle(void) { puts("idle: keep RA session alive while paused"); }

static void refresh_dirty_lists(ra_runtime* runtime) {
  if (runtime->dirty_user) {
    puts("refresh: user score");
    runtime->dirty_user = 0;
  }

  if (runtime->dirty_achievements) {
    puts("refresh: achievements after frame");
    runtime->dirty_achievements = 0;
  }

  if (runtime->dirty_leaderboards) {
    puts("refresh: leaderboards after frame");
    runtime->dirty_leaderboards = 0;
  }
}

static int policy_allows(const ra_runtime* runtime, ra_action action) {
  if (runtime->status != RA_STATUS_ACTIVE || !runtime->hardcore_enabled)
    return 1;

  if (action == RA_ACTION_LOAD_STATE || action == RA_ACTION_FULL_SPEED)
    return 0;

  return 1;
}

static void drain_ui_notifications(ra_runtime* runtime) {
  while (runtime->pending_ui_notifications > 0) {
    puts("ui: show achievement notification from host-owned event data");
    runtime->pending_ui_notifications--;
  }
}

static void login_complete(ra_runtime* runtime, int result) {
  if (result == 0) {
    runtime->status = RA_STATUS_READY;
    puts("login: ready");
  } else {
    runtime->status = RA_STATUS_LOGGED_OUT;
    puts("login: failed");
  }
}

static void load_game_complete(ra_runtime* runtime, int result) {
  if (result == 0) {
    runtime->status = RA_STATUS_ACTIVE;
    puts("load: active");
    puts("refresh: achievements");
    puts("refresh: leaderboards");
    puts("refresh: rich presence");
  } else {
    runtime->status = RA_STATUS_OFFLINE_SESSION;
    puts("load: offline session");
  }
}

static void process_frame(ra_runtime* runtime) {
  if (runtime->status != RA_STATUS_ACTIVE)
    return;

  fake_ra_do_frame(runtime);
  refresh_dirty_lists(runtime);
  drain_ui_notifications(runtime);
}

static void unload_game(ra_runtime* runtime) {
  runtime->generation++;
  runtime->status = RA_STATUS_READY;
  runtime->dirty_achievements = 0;
  runtime->dirty_leaderboards = 0;
  runtime->dirty_user = 0;
  puts("unload: cancel host HTTP, abort rcheevos requests, clear snapshot");
}

static void http_complete(ra_runtime* runtime, uint64_t request_generation) {
  if (request_generation != runtime->generation) {
    puts("http: stale completion discarded");
    return;
  }

  puts("http: complete request into rc_client");
}

int main(void) {
  ra_runtime runtime = {0};
  runtime.status = RA_STATUS_LOGGED_OUT;
  runtime.hardcore_enabled = 1;

  runtime.status = RA_STATUS_AUTHENTICATING;
  login_complete(&runtime, 0);

  runtime.generation++;
  runtime.status = RA_STATUS_LOADING_GAME;
  load_game_complete(&runtime, 0);

  for (int i = 0; i < 5; ++i)
    process_frame(&runtime);

  if (!policy_allows(&runtime, RA_ACTION_LOAD_STATE))
    puts("policy: load state denied while Hardcore RA is active");

  uint64_t old_generation = runtime.generation;
  unload_game(&runtime);
  http_complete(&runtime, old_generation);

  fake_ra_idle();
  return 0;
}
