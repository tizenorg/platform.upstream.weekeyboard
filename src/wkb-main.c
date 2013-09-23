/*
 * Copyright © 2013 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <Eina.h>
#include <Ecore.h>
#include <Ecore_Wayland.h>
#include <Ecore_Evas.h>
#include <Edje.h>

#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "input-method-client-protocol.h"
#include "text-client-protocol.h"

struct weekeyboard
{
   Ecore_Evas *ee;
   Ecore_Wl_Window *win;
   Evas_Object *edje_obj;
   const char *ee_engine;
   char **ignore_keys;

   struct wl_surface *surface;
   struct wl_input_panel *ip;
   struct wl_input_method *im;
   struct wl_output *output;
   struct wl_input_method_context *im_ctx;

   char *surrounding_text;
   char *preedit_str;
   char *language;

   uint32_t text_direction;
   uint32_t preedit_style;
   uint32_t content_hint;
   uint32_t content_purpose;
   uint32_t serial;
   uint32_t surrounding_cursor;

   Eina_Bool context_changed;
};

static void
_cb_wkb_delete_request(Ecore_Evas *ee EINA_UNUSED)
{
   ecore_main_loop_quit();
}

static char *
_wkb_insert_text(const char *text, uint32_t offset, const char *insert)
{
   char *new_text = malloc(strlen(text) + strlen(insert) + 1);

   strncat(new_text, text, offset);
   new_text[offset] = '\0';
   strcat(new_text, insert);
   strcat(new_text, text + offset);

   return new_text;
}

static void
_wkb_commit_preedit_str(struct weekeyboard *wkb)
{
   char *surrounding_text;

   if (!wkb->preedit_str || !strlen(wkb->preedit_str) == 0)
      return;

   wl_input_method_context_cursor_position(wkb->im_ctx, 0, 0);
   wl_input_method_context_commit_string(wkb->im_ctx, wkb->serial, wkb->preedit_str);

   if (wkb->surrounding_text)
     {
        surrounding_text = _wkb_insert_text(wkb->surrounding_text, wkb->surrounding_cursor, wkb->preedit_str);
        free(wkb->surrounding_text);
        wkb->surrounding_text = surrounding_text;
        wkb->surrounding_cursor += strlen(wkb->preedit_str);
     }
   else
     {
        wkb->surrounding_text = strdup(wkb->preedit_str);
        wkb->surrounding_cursor = strlen(wkb->preedit_str);
     }

   free(wkb->preedit_str);
   wkb->preedit_str = strdup("");
}

static void
_wkb_send_preedit_str(struct weekeyboard *wkb, int cursor)
{
   unsigned int index = strlen(wkb->preedit_str);

   if (wkb->preedit_style)
      wl_input_method_context_preedit_styling(wkb->im_ctx, 0, strlen(wkb->preedit_str), wkb->preedit_style);

   if (cursor > 0)
      index = cursor;

   wl_input_method_context_preedit_cursor(wkb->im_ctx, index);
   wl_input_method_context_preedit_string(wkb->im_ctx, wkb->serial, wkb->preedit_str, wkb->preedit_str);
}

static void
_wkb_update_preedit_str(struct weekeyboard *wkb, const char *key)
{
   char *tmp;

   if (!wkb->preedit_str)
      wkb->preedit_str = strdup("");

   tmp = calloc(1, strlen(wkb->preedit_str) + strlen(key) + 1);
   sprintf(tmp, "%s%s", wkb->preedit_str, key);
   free(wkb->preedit_str);
   wkb->preedit_str = tmp;

   if (strcmp(key, " ") == 0)
      _wkb_commit_preedit_str(wkb);
   else
      _wkb_send_preedit_str(wkb, -1);
}

static Eina_Bool
_wkb_ignore_key(struct weekeyboard *wkb, const char *key)
{
   int i;

   if (!wkb->ignore_keys)
       return EINA_FALSE;

   for (i = 0; wkb->ignore_keys[i] != NULL; i++)
      if (!strcmp(key, wkb->ignore_keys[i]))
         return EINA_TRUE;

   return EINA_FALSE;
}

static void
_cb_wkb_on_key_down(void *data, Evas_Object *obj, const char *emission EINA_UNUSED, const char *source)
{
   struct weekeyboard *wkb = data;
   char *src;
   const char *key;

   src = strdup(source);
   key = strtok(src, ":"); /* ignore group */
   key = strtok(NULL, ":");
   if (key == NULL)
       key = ":";

   if (_wkb_ignore_key(wkb, key))
     {
        fprintf(stderr,"Ignoring key '%s'\n", key);
        goto end;
     }
   else if (strcmp(key, "backspace") == 0)
     {
        if (strlen(wkb->preedit_str) == 0)
             wl_input_method_context_delete_surrounding_text(wkb->im_ctx, -1, 1);
        else
          {
             wkb->preedit_str[strlen(wkb->preedit_str) - 1] = '\0';
             _wkb_send_preedit_str(wkb, -1);
          }

        goto end;
     }
   else if (strcmp(key, "enter") == 0)
     {
        _wkb_commit_preedit_str(wkb);
        /* wl_input_method_context_keysym(wkb->im_ctx, wkb->serial, time, XKB_KEY_Return, key_state, mod_mask); */
        goto end;
     }
   else if (strcmp(key, "space") == 0)
     {
        key = " ";
     }

   fprintf(stderr,"KEY = '%s'\n", key);

   _wkb_update_preedit_str(wkb, key);

end:
   free(src);
}

static void
_wkb_im_ctx_surrounding_text(void *data, struct wl_input_method_context *im_ctx, const char *text, uint32_t cursor, uint32_t anchor)
{
   struct weekeyboard *wkb = data;

   fprintf(stderr,"%s()\n", __FUNCTION__);
   free(wkb->surrounding_text);
   wkb->surrounding_text = strdup(text);
   wkb->surrounding_cursor = cursor;
}

static void
_wkb_im_ctx_reset(void *data, struct wl_input_method_context *im_ctx)
{
   struct weekeyboard *wkb = data;

   fprintf(stderr,"%s()\n", __FUNCTION__);

   if (strlen(wkb->preedit_str))
     {
        free(wkb->preedit_str);
        wkb->preedit_str = strdup("");
     }
}

static void
_wkb_im_ctx_content_type(void *data, struct wl_input_method_context *im_ctx, uint32_t hint, uint32_t purpose)
{
   struct weekeyboard *wkb = data;

   fprintf(stderr,"%s(): im_context = %p hint = %d purpose = %d\n", __FUNCTION__, im_ctx, hint, purpose);

   if (!wkb->context_changed)
      return;

   switch (purpose)
     {
      case WL_TEXT_INPUT_CONTENT_PURPOSE_DIGITS:
      case WL_TEXT_INPUT_CONTENT_PURPOSE_NUMBER:
           {
              edje_object_signal_emit(wkb->edje_obj, "show,numeric", "");
              break;
           }
      default:
           {
              edje_object_signal_emit(wkb->edje_obj, "show,alphanumeric", "");
              break;
           }
     }

   wkb->content_hint = hint;
   wkb->content_purpose = purpose;

   wkb->context_changed = EINA_FALSE;
}

static void
_wkb_im_ctx_invoke_action(void *data, struct wl_input_method_context *im_ctx, uint32_t button, uint32_t index)
{
   struct weekeyboard *wkb = data;

   fprintf(stderr,"%s()\n", __FUNCTION__);
   if (button != BTN_LEFT)
      return;

   _wkb_send_preedit_str(wkb, index);
}

static void
_wkb_im_ctx_commit_state(void *data, struct wl_input_method_context *im_ctx, uint32_t serial)
{
   struct weekeyboard *wkb = data;

   fprintf(stderr,"%s()\n", __FUNCTION__);
   if (wkb->surrounding_text)
      fprintf(stderr, "Surrounding text updated: %s\n", wkb->surrounding_text);

   wkb->serial = serial;
   /* FIXME */
   wl_input_method_context_language(im_ctx, wkb->serial, "en");//wkb->language);
   wl_input_method_context_text_direction(im_ctx, wkb->serial, WL_TEXT_INPUT_TEXT_DIRECTION_LTR);//wkb->text_direction);
}

static void
_wkb_im_ctx_preferred_language(void *data, struct wl_input_method_context *im_ctx, const char *language)
{
   struct weekeyboard *wkb = data;

   if (language && wkb->language && !strcmp(language, wkb->language))
      return;

   if (wkb->language)
     {
        free(wkb->language);
        wkb->language = NULL;
     }

   if (language)
     {
        wkb->language = strdup(language);
        fprintf(stderr,"Language changed, new: '%s\n", language);
     }
}

static const struct wl_input_method_context_listener wkb_im_context_listener = {
     _wkb_im_ctx_surrounding_text,
     _wkb_im_ctx_reset,
     _wkb_im_ctx_content_type,
     _wkb_im_ctx_invoke_action,
     _wkb_im_ctx_commit_state,
     _wkb_im_ctx_preferred_language,
};

static void
_wkb_im_activate(void *data, struct wl_input_method *input_method, struct wl_input_method_context *im_ctx)
{
   struct weekeyboard *wkb = data;
   struct wl_array modifiers_map;

   if (wkb->im_ctx)
      wl_input_method_context_destroy(wkb->im_ctx);

   if (wkb->preedit_str)
      free(wkb->preedit_str);

   wkb->preedit_str = strdup("");
   wkb->content_hint = WL_TEXT_INPUT_CONTENT_HINT_NONE;
   wkb->content_purpose = WL_TEXT_INPUT_CONTENT_PURPOSE_NORMAL;
   free(wkb->language);
   wkb->language = NULL;
   free(wkb->surrounding_text);
   wkb->surrounding_text = NULL;
   wkb->serial = 0;

   wkb->im_ctx = im_ctx;
   wl_input_method_context_add_listener(im_ctx, &wkb_im_context_listener, wkb);

   /*
   wl_array_init(&modifiers_map);

   keysym_modifiers_add(&modifiers_map, "Shift");
   keysym_modifiers_add(&modifiers_map, "Control");
   keysym_modifiers_add(&modifiers_map, "Mod1");

   wl_input_method_context_modifiers_map(im_ctx, &modifiers_map);

   wkb->keysym.shift_mask = keysym_modifiers_get_mask(&modifiers_map, "Shift");

   wl_array_release(&modifiers_map);
   */

   /* FIXME */
   wl_input_method_context_language(im_ctx, wkb->serial, "en");//wkb->language);
   wl_input_method_context_text_direction(im_ctx, wkb->serial, WL_TEXT_INPUT_TEXT_DIRECTION_LTR);//wkb->text_direction);

   wkb->context_changed = EINA_TRUE;
   evas_object_show(wkb->edje_obj);
}

static void
_wkb_im_deactivate(void *data, struct wl_input_method *input_method, struct wl_input_method_context *im_ctx)
{
   struct weekeyboard *wkb = data;

   if (!wkb->im_ctx)
      return;

   wl_input_method_context_destroy(wkb->im_ctx);
   wkb->im_ctx = NULL;
   evas_object_hide(wkb->edje_obj);
}

static const struct wl_input_method_listener wkb_im_listener = {
     _wkb_im_activate,
     _wkb_im_deactivate
};


static Eina_Bool
_wkb_ui_setup(struct weekeyboard *wkb)
{
   char path[PATH_MAX];
   Evas *evas;
   Evas_Coord w, h;
   char *ignore_keys;

   ecore_evas_alpha_set(wkb->ee, EINA_TRUE);
   ecore_evas_title_set(wkb->ee, "EFL virtual keyboard");

   evas = ecore_evas_get(wkb->ee);
   wkb->edje_obj = edje_object_add(evas);
   /*ecore_evas_object_associate(wkb->ee, edje_obj, ECORE_EVAS_OBJECT_ASSOCIATE_BASE);*/

   /* Check which theme we should use according to the screen width */
   ecore_wl_screen_size_get(&w, &h);
   if (w >= 720)
      w = 720;
   else
      w = 600;

   sprintf(path, PKGDATADIR"/default_%d.edj", w);
   fprintf(stderr,"Loading edje file: '%s'\n", path);

   if (!edje_object_file_set(wkb->edje_obj, path, "main"))
     {
        int err = edje_object_load_error_get(wkb->edje_obj);
        fprintf(stderr, "error loading the edje file:%s\n", edje_load_error_str(err));
        return EINA_FALSE;
     }

   edje_object_size_min_get(wkb->edje_obj, &w, &h);
   if (w == 0 || h == 0)
     {
        edje_object_size_min_restricted_calc(wkb->edje_obj, &w, &h, w, h);
        if (w == 0 || h == 0)
           edje_object_parts_extends_calc(wkb->edje_obj, NULL, NULL, &w, &h);
     }

   ecore_evas_move_resize(wkb->ee, 0, 0, w, h);
   evas_object_move(wkb->edje_obj, 0, 0);
   evas_object_resize(wkb->edje_obj, w, h);
   evas_object_size_hint_min_set(wkb->edje_obj, w, h);
   evas_object_size_hint_max_set(wkb->edje_obj, w, h);

   edje_object_signal_callback_add(wkb->edje_obj, "key_down", "*", _cb_wkb_on_key_down, wkb);
   ecore_evas_callback_delete_request_set(wkb->ee, _cb_wkb_delete_request);

   /*
    * The keyboard surface is bigger than it appears so that we can show the
    * key pressed animation without requiring the use of subsurfaces. Here we
    * resize the input region of the surface to match the keyboard background
    * image, so that we can pass mouse events to the surfaces that may be
    * located below the keyboard.
    */
   if (wkb->win)
     {
        int x, y, w, h;
        struct wl_region *input = wl_compositor_create_region(wkb->win->display->wl.compositor);

        edje_object_part_geometry_get(wkb->edje_obj, "background", &x, &y, &w, &h);
        wl_region_add(input, x, y, w, h);
        wl_surface_set_input_region(wkb->surface, input);
        wl_region_destroy(input);
     }

   /* special keys */
   ignore_keys = edje_file_data_get(path, "ignore-keys");
   if (!ignore_keys)
     {
        fprintf(stderr,"Special keys file not found in '%s'\n", path);
        goto end;
     }

   fprintf(stderr,"Got ignore keys = %s\n", ignore_keys);
   wkb->ignore_keys = eina_str_split(ignore_keys, "\n", 0);
   free(ignore_keys);

end:
   ecore_evas_show(wkb->ee);
   return EINA_TRUE;
}

static void
_wkb_setup(struct weekeyboard *wkb)
{
   struct wl_list *globals;
   struct wl_registry *registry;
   Ecore_Wl_Global *global;

   struct wl_input_panel_surface *ips;

   globals = ecore_wl_globals_get();
   registry = ecore_wl_registry_get();
   wl_list_for_each(global, globals, link)
     {
        if (strcmp(global->interface, "wl_input_panel") == 0)
           wkb->ip = wl_registry_bind(registry, global->id, &wl_input_panel_interface, 1);
        else if (strcmp(global->interface, "wl_input_method") == 0)
           wkb->im = wl_registry_bind(registry, global->id, &wl_input_method_interface, 1);
        else if (strcmp(global->interface, "wl_output") == 0)
           wkb->output = wl_registry_bind(registry, global->id, &wl_output_interface, 1);
     }

   /* Set input panel surface */
   wkb->win = ecore_evas_wayland_window_get(wkb->ee);
   ecore_wl_window_type_set(wkb->win, ECORE_WL_WINDOW_TYPE_NONE);
   wkb->surface = ecore_wl_window_surface_create(wkb->win);
   ips = wl_input_panel_get_input_panel_surface(wkb->ip, wkb->surface);
   wl_input_panel_surface_set_toplevel(ips, wkb->output, WL_INPUT_PANEL_SURFACE_POSITION_CENTER_BOTTOM);

   /* Input method listener */
   wl_input_method_add_listener(wkb->im, &wkb_im_listener, wkb);
}

static void
_wkb_free(struct weekeyboard *wkb)
{
   if (wkb->im_ctx)
      wl_input_method_context_destroy(wkb->im_ctx);

   if (wkb->ignore_keys)
     {
        free(*wkb->ignore_keys);
        free(wkb->ignore_keys);
     }

   evas_object_del(wkb->edje_obj);
   free(wkb->preedit_str);
   free(wkb->surrounding_text);
}

static Eina_Bool
_wkb_check_evas_engine(struct weekeyboard *wkb)
{
   Eina_Bool ret = EINA_FALSE;
   char *env = getenv("ECORE_EVAS_ENGINE");

   if (!env)
     {
        if (ecore_evas_engine_type_supported_get(ECORE_EVAS_ENGINE_WAYLAND_SHM))
           env = "wayland_shm";
        else if (ecore_evas_engine_type_supported_get(ECORE_EVAS_ENGINE_WAYLAND_EGL))
           env = "wayland_egl";
        else
          {
             fprintf(stderr,"ERROR: Ecore_Evas does must be compiled with support for Wayland engines\n");
             goto err;
          }
     }
   else if (strcmp(env, "wayland_shm") != 0 && strcmp(env, "wayland_egl") != 0)
     {
        fprintf(stderr,"ERROR: ECORE_EVAS_ENGINE must be set to either 'wayland_shm' or 'wayland_egl'\n");
        goto err;
     }

   wkb->ee_engine = env;
   ret = EINA_TRUE;

err:
   return ret;
}

int
main(int argc EINA_UNUSED, char **argv EINA_UNUSED)
{
   struct weekeyboard wkb = {0};
   int ret = EXIT_FAILURE;

   if (!ecore_init())
      return ret;

   if (!ecore_evas_init())
      goto ecore_err;

   if (!edje_init())
      goto ee_err;

   if (!_wkb_check_evas_engine(&wkb))
      goto edj_err;

   fprintf(stderr,"SELECTED ENGINE = %s\n", wkb.ee_engine);
   wkb.ee = ecore_evas_new(wkb.ee_engine, 0, 0, 1, 1, "frame=0");

   if (!wkb.ee)
     {
        fprintf(stderr,"ERROR: Unable to create Ecore_Evas object\n");
        goto edj_err;
     }

   _wkb_setup(&wkb);

   if (!_wkb_ui_setup(&wkb))
      goto end;

   ecore_main_loop_begin();

   ret = EXIT_SUCCESS;

   _wkb_free(&wkb);

end:
   ecore_evas_free(wkb.ee);

edj_err:
   edje_shutdown();

ee_err:
   ecore_evas_shutdown();

ecore_err:
   ecore_shutdown();

   return ret;
}
