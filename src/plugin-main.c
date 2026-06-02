#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>
#include <util/platform.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

#define SIGNATURE_SOURCE "Multi Live Signature"
#define ALERTS_SOURCE "Multi Live Alerts"
#define BROWSER_SOURCE_ID "browser_source"
#define PANEL_BASE_URL "https://afs-multichat.duckdns.org/multilive"
#define DEFAULT_SLUG "arfustv"
#define SIGNATURE_WIDTH 560
#define SIGNATURE_HEIGHT 80
#define CANVAS_WIDTH 1920
#define CANVAS_HEIGHT 1080

static float tick_accumulator = 0.0f;
static char widget_token[256] = "";
static char streamer_slug[96] = DEFAULT_SLUG;

static void copy_clean(char *dst, size_t dst_size, const char *src)
{
	if (!dst || dst_size == 0)
		return;
	if (!src)
		src = "";
	snprintf(dst, dst_size, "%s", src);
	dst[dst_size - 1] = '\0';
}

static void extract_query_value(char *dst, size_t dst_size, const char *url, const char *key)
{
	if (!url || !key || !dst || dst_size == 0 || dst[0] != '\0')
		return;

	char pattern[64];
	snprintf(pattern, sizeof(pattern), "%s=", key);
	const char *match = strstr(url, pattern);
	if (!match)
		return;
	match += strlen(pattern);

	size_t i = 0;
	while (*match && *match != '&' && *match != '#' && *match != '"' && *match != '\'' && *match != '\\' &&
	       *match != ',' && *match != '}' && *match != ']' && i + 1 < dst_size) {
		dst[i++] = *match++;
	}
	dst[i] = '\0';
}

static void extract_slug(char *dst, size_t dst_size, const char *url)
{
	if (!url || !dst || dst_size == 0)
		return;
	const char *marker = strstr(url, "/multilive/");
	if (!marker)
		return;
	marker += strlen("/multilive/");
	size_t i = 0;
	while (*marker && *marker != '/' && i + 1 < dst_size) {
		dst[i++] = *marker++;
	}
	dst[i] = '\0';
}

static void source_url(char *dst, size_t dst_size, obs_source_t *source)
{
	if (!source) {
		copy_clean(dst, dst_size, "");
		return;
	}
	obs_data_t *settings = obs_source_get_settings(source);
	copy_clean(dst, dst_size, obs_data_get_string(settings, "url"));
	obs_data_release(settings);
}

static bool absorb_context_item(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	UNUSED_PARAMETER(scene);
	UNUSED_PARAMETER(param);

	obs_source_t *source = obs_sceneitem_get_source(item);
	char url[1024] = "";
	source_url(url, sizeof(url), source);

	extract_query_value(widget_token, sizeof(widget_token), url, "widgetToken");
	extract_query_value(widget_token, sizeof(widget_token), url, "token");
	if (strcmp(streamer_slug, DEFAULT_SLUG) == 0)
		extract_slug(streamer_slug, sizeof(streamer_slug), url);

	return true;
}

static void absorb_existing_signature_context(obs_scene_t *scene)
{
	obs_scene_enum_items(scene, absorb_context_item, NULL);
}

static void absorb_user_ini_context(void)
{
	char path[1024] = "";
#ifdef _WIN32
	const char *appdata = getenv("APPDATA");
	if (!appdata)
		return;
	snprintf(path, sizeof(path), "%s\\obs-studio\\user.ini", appdata);
#else
	if (os_get_config_path(path, sizeof(path), "obs-studio/user.ini") <= 0)
		return;
#endif

	FILE *file = fopen(path, "r");
	if (!file)
		return;

	char line[8192];
	while (fgets(line, sizeof(line), file)) {
		if (!strstr(line, "widgetToken=") && !strstr(line, "token="))
			continue;

		extract_query_value(widget_token, sizeof(widget_token), line, "widgetToken");
		extract_query_value(widget_token, sizeof(widget_token), line, "token");
		if (strcmp(streamer_slug, DEFAULT_SLUG) == 0)
			extract_slug(streamer_slug, sizeof(streamer_slug), line);

		if (widget_token[0] != '\0' && strcmp(streamer_slug, DEFAULT_SLUG) != 0)
			break;
	}

	fclose(file);
}

static void signature_url(char *dst, size_t dst_size)
{
	if (widget_token[0] == '\0') {
		snprintf(dst, dst_size, "%s/%s/widgets/signature/", PANEL_BASE_URL, streamer_slug);
		return;
	}
	snprintf(dst, dst_size, "%s/%s/widgets/signature/?widgetToken=%s", PANEL_BASE_URL, streamer_slug, widget_token);
}

static void update_browser_source(obs_source_t *source, const char *url)
{
	obs_data_t *current = obs_source_get_settings(source);
	const char *current_url = obs_data_get_string(current, "url");
	long long current_width = obs_data_get_int(current, "width");
	long long current_height = obs_data_get_int(current, "height");
	bool current_shutdown = obs_data_get_bool(current, "shutdown");
	bool current_restart_when_active = obs_data_get_bool(current, "restart_when_active");

	bool already_ok = current_url && strcmp(current_url, url) == 0 && current_width == SIGNATURE_WIDTH &&
			  current_height == SIGNATURE_HEIGHT && !current_shutdown && current_restart_when_active;
	obs_data_release(current);
	if (already_ok)
		return;

	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "url", url);
	obs_data_set_int(settings, "width", SIGNATURE_WIDTH);
	obs_data_set_int(settings, "height", SIGNATURE_HEIGHT);
	obs_data_set_bool(settings, "shutdown", false);
	obs_data_set_bool(settings, "restart_when_active", true);
	obs_source_update(source, settings);
	obs_data_release(settings);
}

static obs_source_t *create_signature_source(const char *url)
{
	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "url", url);
	obs_data_set_int(settings, "width", SIGNATURE_WIDTH);
	obs_data_set_int(settings, "height", SIGNATURE_HEIGHT);
	obs_data_set_bool(settings, "shutdown", false);
	obs_data_set_bool(settings, "restart_when_active", true);

	obs_source_t *source = obs_source_create(BROWSER_SOURCE_ID, SIGNATURE_SOURCE, settings, NULL);
	obs_data_release(settings);
	return source;
}

static bool remove_duplicate_alert_item(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	UNUSED_PARAMETER(scene);
	UNUSED_PARAMETER(param);

	obs_source_t *source = obs_sceneitem_get_source(item);
	const char *name = obs_source_get_name(source);
	char url[1024] = "";
	source_url(url, sizeof(url), source);
	if (name && strcmp(name, ALERTS_SOURCE) != 0 && strstr(url, "/alerts/")) {
		obs_log(LOG_INFO, "removing duplicate alerts source: %s", name);
		obs_sceneitem_remove(item);
	}

	return true;
}

static void remove_duplicate_alert_items(obs_scene_t *scene)
{
	obs_scene_enum_items(scene, remove_duplicate_alert_item, NULL);
}

static void ensure_signature(void)
{
	obs_source_t *scene_source = obs_frontend_get_current_scene();
	if (!scene_source)
		return;

	obs_scene_t *scene = obs_scene_from_source(scene_source);
	if (!scene) {
		obs_source_release(scene_source);
		return;
	}

	absorb_user_ini_context();
	absorb_existing_signature_context(scene);

	char url[1200] = "";
	signature_url(url, sizeof(url));

	obs_source_t *signature = obs_get_source_by_name(SIGNATURE_SOURCE);
	if (signature) {
		update_browser_source(signature, url);
	} else {
		signature = create_signature_source(url);
	}

	if (!signature) {
		obs_source_release(scene_source);
		return;
	}

	obs_sceneitem_t *item = obs_scene_find_source(scene, SIGNATURE_SOURCE);
	if (!item)
		item = obs_scene_add(scene, signature);

	if (item) {
		struct vec2 pos;
		vec2_set(&pos, (float)(CANVAS_WIDTH - SIGNATURE_WIDTH - 12), (float)(CANVAS_HEIGHT - SIGNATURE_HEIGHT - 8));
		obs_sceneitem_set_visible(item, true);
		obs_sceneitem_set_locked(item, true);
		obs_sceneitem_set_pos(item, &pos);
		obs_sceneitem_set_order(item, OBS_ORDER_MOVE_TOP);
	}

	remove_duplicate_alert_items(scene);
	obs_source_release(signature);
	obs_source_release(scene_source);
}

static void tick(void *data, float seconds)
{
	UNUSED_PARAMETER(data);
	tick_accumulator += seconds;
	if (tick_accumulator < 2.0f)
		return;
	tick_accumulator = 0.0f;
	ensure_signature();
}

static void frontend_event(enum obs_frontend_event event, void *data)
{
	UNUSED_PARAMETER(data);
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING || event == OBS_FRONTEND_EVENT_SCENE_CHANGED ||
	    event == OBS_FRONTEND_EVENT_STREAMING_STARTED) {
		ensure_signature();
	}
}

bool obs_module_load(void)
{
	obs_log(LOG_INFO, "Multi Live Signature Guard loaded (version %s)", PLUGIN_VERSION);
	obs_frontend_add_event_callback(frontend_event, NULL);
	obs_add_tick_callback(tick, NULL);
	return true;
}

void obs_module_unload(void)
{
	obs_remove_tick_callback(tick, NULL);
	obs_frontend_remove_event_callback(frontend_event, NULL);
	obs_log(LOG_INFO, "Multi Live Signature Guard unloaded");
}
