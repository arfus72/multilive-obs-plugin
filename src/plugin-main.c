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
#define PANEL_BASE_URL "https://multi-live-pro.com/multilive"
#define DEFAULT_SLUG "arfustv"
#define LIVE_DOCK_TITLE "Multi Live Dock Live"
#define CHAT_DOCK_TITLE "Multi Live Chat"
#define LIVE_DOCK_UUID "multilive_live_dock"
#define CHAT_DOCK_UUID "multilive_chat_dock"
#define SIGNATURE_WIDTH 560
#define SIGNATURE_HEIGHT 80
#define CANVAS_WIDTH 1920
#define CANVAS_HEIGHT 1080
#define PLUGIN_DISPLAY_VERSION "1.2.0"

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
	extract_query_value(widget_token, sizeof(widget_token), url, "widgettoken");
	extract_query_value(widget_token, sizeof(widget_token), url, "token");
	if (strcmp(streamer_slug, DEFAULT_SLUG) == 0)
		extract_slug(streamer_slug, sizeof(streamer_slug), url);

	return true;
}

static void absorb_existing_signature_context(obs_scene_t *scene)
{
	obs_scene_enum_items(scene, absorb_context_item, NULL);
}

static bool obs_user_ini_path(char *path, size_t path_size)
{
#ifdef _WIN32
	const char *appdata = getenv("APPDATA");
	if (!appdata)
		return false;
	snprintf(path, path_size, "%s\\obs-studio\\user.ini", appdata);
#else
	if (os_get_config_path(path, path_size, "obs-studio/user.ini") <= 0)
		return false;
#endif
	return true;
}

static void absorb_user_ini_context(void)
{
	char path[1024] = "";
	if (!obs_user_ini_path(path, sizeof(path)))
		return;

	FILE *file = fopen(path, "r");
	if (!file)
		return;

	char line[8192];
	while (fgets(line, sizeof(line), file)) {
		if (!strstr(line, "widgetToken=") && !strstr(line, "widgettoken=") && !strstr(line, "token="))
			continue;

		extract_query_value(widget_token, sizeof(widget_token), line, "widgetToken");
		extract_query_value(widget_token, sizeof(widget_token), line, "widgettoken");
		extract_query_value(widget_token, sizeof(widget_token), line, "token");
		if (strcmp(streamer_slug, DEFAULT_SLUG) == 0)
			extract_slug(streamer_slug, sizeof(streamer_slug), line);

		if (widget_token[0] != '\0' && strcmp(streamer_slug, DEFAULT_SLUG) != 0)
			break;
	}

	fclose(file);
}

static void dock_url(char *dst, size_t dst_size, const char *dock_path)
{
	if (widget_token[0] == '\0') {
		snprintf(dst, dst_size, "%s/%s/%s?dock=1&pluginVersion=%s", PANEL_BASE_URL, streamer_slug, dock_path,
			 PLUGIN_DISPLAY_VERSION);
		return;
	}
	snprintf(dst, dst_size, "%s/%s/%s?dock=1&pluginVersion=%s&widgetToken=%s", PANEL_BASE_URL, streamer_slug,
		 dock_path, PLUGIN_DISPLAY_VERSION, widget_token);
}

static void dock_config_json(char *dst, size_t dst_size)
{
	char live_url[1200] = "";
	char chat_url[1200] = "";
	dock_url(live_url, sizeof(live_url), "live-dock/");
	dock_url(chat_url, sizeof(chat_url), "chat/");
	snprintf(dst, dst_size,
		 "[{\"title\":\"%s\",\"url\":\"%s\",\"uuid\":\"%s\"},{\"title\":\"%s\",\"url\":\"%s\",\"uuid\":\"%s\"}]",
		 LIVE_DOCK_TITLE, live_url, LIVE_DOCK_UUID, CHAT_DOCK_TITLE, chat_url, CHAT_DOCK_UUID);
}

static bool read_file_text(const char *path, char **out_content, long *out_size)
{
	*out_content = NULL;
	*out_size = 0;

	FILE *file = fopen(path, "rb");
	if (!file)
		return false;

	if (fseek(file, 0, SEEK_END) != 0) {
		fclose(file);
		return false;
	}
	long size = ftell(file);
	if (size < 0) {
		fclose(file);
		return false;
	}
	rewind(file);

	char *content = (char *)malloc((size_t)size + 1);
	if (!content) {
		fclose(file);
		return false;
	}

	size_t read = fread(content, 1, (size_t)size, file);
	fclose(file);
	content[read] = '\0';
	*out_content = content;
	*out_size = (long)read;
	return true;
}

static void write_ini_line(FILE *out, const char *start, size_t len)
{
	fwrite(start, 1, len, out);
	fputc('\n', out);
}

static void repair_obs_docks(void)
{
	if (widget_token[0] == '\0')
		return;

	char path[1024] = "";
	if (!obs_user_ini_path(path, sizeof(path)))
		return;

	char docks_json[3200] = "";
	dock_config_json(docks_json, sizeof(docks_json));

	char expected_line[3400] = "";
	snprintf(expected_line, sizeof(expected_line), "ExtraBrowserDocks=%s", docks_json);

	char *content = NULL;
	long size = 0;
	if (read_file_text(path, &content, &size) && strstr(content, expected_line)) {
		free(content);
		return;
	}

	char tmp_path[1100] = "";
	snprintf(tmp_path, sizeof(tmp_path), "%s.multilive.tmp", path);
	FILE *out = fopen(tmp_path, "wb");
	if (!out) {
		free(content);
		return;
	}

	bool in_basic = false;
	bool found_basic = false;
	bool wrote_docks = false;

	if (content) {
		const char *cursor = content;
		while (*cursor) {
			const char *line_start = cursor;
			const char *line_end = strchr(cursor, '\n');
			if (!line_end)
				line_end = cursor + strlen(cursor);
			cursor = *line_end == '\n' ? line_end + 1 : line_end;

			size_t len = (size_t)(line_end - line_start);
			if (len > 0 && line_start[len - 1] == '\r')
				len--;

			bool is_section = len > 0 && line_start[0] == '[';
			if (is_section && in_basic && !wrote_docks) {
				fprintf(out, "%s\n", expected_line);
				wrote_docks = true;
			}

			if (is_section) {
				in_basic = len == strlen("[BasicWindow]") && strncmp(line_start, "[BasicWindow]", len) == 0;
				if (in_basic)
					found_basic = true;
			}

			if (in_basic && len >= strlen("ExtraBrowserDocks=") &&
			    strncmp(line_start, "ExtraBrowserDocks=", strlen("ExtraBrowserDocks=")) == 0) {
				if (!wrote_docks) {
					fprintf(out, "%s\n", expected_line);
					wrote_docks = true;
				}
				continue;
			}

			write_ini_line(out, line_start, len);
		}
	}

	if (!found_basic) {
		fprintf(out, "\n[BasicWindow]\n%s\n", expected_line);
	} else if (in_basic && !wrote_docks) {
		fprintf(out, "%s\n", expected_line);
	}

	fclose(out);
	free(content);

	char bak_path[1100] = "";
	snprintf(bak_path, sizeof(bak_path), "%s.multilive.bak", path);
	remove(bak_path);

	if (rename(path, bak_path) != 0 || rename(tmp_path, path) != 0) {
		rename(bak_path, path);
		remove(tmp_path);
		obs_log(LOG_WARNING, "Multi Live: OBS docks update failed. Close OBS and run the installer as administrator.");
		return;
	}

	remove(bak_path);

	obs_log(LOG_INFO, "Multi Live: OBS docks updated for %s. Restart OBS if the old login docks are still open.",
		streamer_slug);
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
	repair_obs_docks();

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
	obs_log(LOG_INFO, "Multi Live Signature Guard loaded (version %s)", PLUGIN_DISPLAY_VERSION);
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
