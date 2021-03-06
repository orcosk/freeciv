/***********************************************************************
 Freeciv - Copyright (C) 1996 - A Kjeldberg, L Gregersen, P Unold
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

/* SDL2 */
#ifdef SDL2_PLAIN_INCLUDE
#include <SDL.h>
#else  /* SDL2_PLAIN_INCLUDE */
#include <SDL2/SDL.h>
#endif /* SDL2_PLAIN_INCLUDE */

/* utility */
#include "fcintl.h"
#include "log.h"

/* common */
#include "game.h"
#include "government.h"
#include "movement.h"

/* client */
#include "client_main.h"
#include "helpdata.h"

/* gui-sdl2 */
#include "colors.h"
#include "graphics.h"
#include "gui_id.h"
#include "gui_main.h"
#include "gui_tilespec.h"
#include "mapview.h"
#include "repodlgs.h"
#include "sprite.h"
#include "themespec.h"
#include "widget.h"

#include "helpdlg.h"

static struct advanced_dialog *help_dlg = NULL;

struct techs_buttons {
  struct widget *pTargets[6], *pSub_Targets[6];
  struct widget *pRequirementButton[2], *pSub_Req[4];
  struct widget *dock;
  bool show_tree;
  bool show_full_tree;
};

struct units_buttons {
  struct widget *pObsoleteByButton;
  struct widget *pRequirementButton;
  struct widget *dock;
};

enum help_page_type current_help_dlg = HELP_LAST;

static const int bufsz = 8192;

static int change_tech_callback(struct widget *pwidget);

/**********************************************************************//**
  Open Help Browser without any specific topic in mind
**************************************************************************/
void popup_help_browser(void)
{
  popup_tech_info(A_NONE);
}

/**********************************************************************//**
  Popup the help dialog to get help on the given string topic.  Note that
  the topic may appear in multiple sections of the help (it may be both
  an improvement and a unit, for example).

  The string will be untranslated.
**************************************************************************/
void popup_help_dialog_string(const char *item)
{
  popup_help_dialog_typed(Q_(item), HELP_ANY);
}

/**********************************************************************//**
  Popup the help dialog to display help on the given string topic from
  the given section.

  The string will be translated.
**************************************************************************/
void popup_help_dialog_typed(const char *item, enum help_page_type eHPT)
{
  log_debug("popup_help_dialog_typed : PORT ME");
}

/**********************************************************************//**
  Close the help dialog.
**************************************************************************/
void popdown_help_dialog(void)
{
  if (help_dlg) {
    popdown_window_group_dialog(help_dlg->begin_widget_list,
                                help_dlg->end_widget_list);
    FC_FREE(help_dlg->pScroll);
    FC_FREE(help_dlg);
    current_help_dlg = HELP_LAST;
  }
}

/**********************************************************************//**
  User interacted with help dialog window
**************************************************************************/
static int help_dlg_window_callback(struct widget *pwindow)
{
  return -1;
}

/**********************************************************************//**
  User requested closing of the help dialog
**************************************************************************/
static int exit_help_dlg_callback(struct widget *pwidget)
{
  if (PRESSED_EVENT(main_data.event)) {
    popdown_help_dialog();
    flush_dirty();
  }

  return -1;
}

/**********************************************************************//**
  User requested new government help
**************************************************************************/
static int change_gov_callback(struct widget *pwidget)
{
  if (PRESSED_EVENT(main_data.event)) {
    popup_gov_info(MAX_ID - pwidget->ID);
  }

  return -1;
}

/**********************************************************************//**
  Show government info
**************************************************************************/
void popup_gov_info(int gov)
{
}

/**********************************************************************//**
  User requested new improvement help
**************************************************************************/
static int change_impr_callback(struct widget *pwidget)
{
  if (PRESSED_EVENT(main_data.event)) {
    popup_impr_info(MAX_ID - pwidget->ID);
  }

  return -1;
}

/**********************************************************************//**
  Refresh improvement help dialog
**************************************************************************/
static void redraw_impr_info_dlg(void)
{
  SDL_Color bg_color = {255, 255, 255, 64};
  struct widget *pwindow = help_dlg->end_widget_list;
  struct units_buttons *store = (struct units_buttons *)pwindow->data.ptr;
  SDL_Rect dst;

  redraw_group(pwindow->prev, pwindow, FALSE);

  dst.x = store->dock->prev->size.x - adj_size(10);
  dst.y = store->dock->prev->size.y - adj_size(10);
  dst.w = pwindow->size.w - (dst.x - pwindow->size.x) - adj_size(10);
  dst.h = pwindow->size.h - (dst.y - pwindow->size.y) - adj_size(10);

  fill_rect_alpha(pwindow->dst->surface, &dst, &bg_color);

  create_frame(pwindow->dst->surface,
               dst.x, dst.y, dst.w, dst.h,
               get_theme_color(COLOR_THEME_HELPDLG_FRAME));

  /*------------------------------------- */
  redraw_group(help_dlg->begin_widget_list, pwindow->prev->prev, FALSE);
  widget_flush(pwindow);
}

/**********************************************************************//**
  Show improvement info
**************************************************************************/
void popup_impr_info(Impr_type_id impr)
{
  SDL_Color bg_color = {255, 255, 255, 128};
  struct widget *pwindow;
  struct units_buttons *store;
  struct widget *pCloseButton = NULL;
  struct widget *pListToggleButton = NULL;
  struct widget *improvement_button = NULL;
  struct widget *pImprNameLabel = NULL;
  struct widget *pCostLabel = NULL;
  struct widget *pUpkeepLabel = NULL;
  struct widget *pRequirementLabel = NULL;
  struct widget *pRequirementLabel2 = NULL;
  struct widget *pObsoleteByLabel = NULL;
  struct widget *pObsoleteByLabel2 = NULL;
  struct widget *pHelptextLabel = NULL;
  struct widget *dock;
  utf8_str *title;
  utf8_str *pstr;
  SDL_Surface *surf;
  int h, start_x, start_y, impr_type_count;
  bool created, text = FALSE;
  int scrollbar_width = 0;
  struct impr_type *pimpr_type;
  char buffer[64000];
  SDL_Rect area;
  struct advance *obsTech = NULL;

  if (current_help_dlg != HELP_IMPROVEMENT) {
    popdown_help_dialog();
  }

  if (!help_dlg) {
    SDL_Surface *background_tmpl, *background, *pText, *icon;
    SDL_Rect dst;

    current_help_dlg = HELP_IMPROVEMENT;
    created = TRUE;

    /* create dialog */
    help_dlg = fc_calloc(1, sizeof(struct advanced_dialog));
    store = fc_calloc(1, sizeof(struct units_buttons));

    /* create window */
    title = create_utf8_from_char(_("Help : Improvements"), adj_font(12));
    title->style |= TTF_STYLE_BOLD;

    pwindow = create_window_skeleton(NULL, title, WF_FREE_DATA);
    pwindow->action = help_dlg_window_callback;
    set_wstate(pwindow , FC_WS_NORMAL);
    pwindow->data.ptr = (void *)store;
    add_to_gui_list(ID_WINDOW, pwindow);

    help_dlg->end_widget_list = pwindow;

    area = pwindow->area;
    /* ------------------ */

    /* close button */
    pCloseButton = create_themeicon(current_theme->Small_CANCEL_Icon, pwindow->dst,
                                    WF_WIDGET_HAS_INFO_LABEL
                                    | WF_RESTORE_BACKGROUND);
    pCloseButton->info_label =
        create_utf8_from_char(_("Close Dialog (Esc)"), adj_font(12));
    pCloseButton->action = exit_help_dlg_callback;
    set_wstate(pCloseButton, FC_WS_NORMAL);
    pCloseButton->key = SDLK_ESCAPE;

    add_to_gui_list(ID_BUTTON, pCloseButton);

    /* ------------------ */
    dock = pCloseButton;

    pstr = create_utf8_str(NULL, 0, adj_font(10));
    pstr->style |= (TTF_STYLE_BOLD | SF_CENTER);

    /* background template for entries in scroll list */
    background_tmpl = create_surf(adj_size(135), adj_size(40), SDL_SWSURFACE);
    SDL_FillRect(background_tmpl, NULL, map_rgba(background_tmpl->format, bg_color));

    create_frame(background_tmpl,
                 0, 0, background_tmpl->w - 1, background_tmpl->h - 1,
                 get_theme_color(COLOR_THEME_HELPDLG_FRAME));

    impr_type_count = 0;
    improvement_iterate(pimprove) {

      /* copy background surface */  
      background = copy_surface(background_tmpl);

      /* blit improvement name */
      copy_chars_to_utf8_str(pstr, improvement_name_translation(pimprove));
      pText = create_text_surf_smaller_than_w(pstr, adj_size(100 - 4));
      dst.x = adj_size(40) + (background->w - pText->w - adj_size(40)) / 2;
      dst.y = (background->h - pText->h) / 2;
      alphablit(pText, NULL, background, &dst, 255);
      FREESURFACE(pText);

      /* blit improvement icon */
      icon = ResizeSurfaceBox(get_building_surface(pimprove),
                              adj_size(36), adj_size(36), 1, TRUE, TRUE);
      dst.x = adj_size(5);
      dst.y = (background->h - icon->h) / 2;
      alphablit(icon, NULL, background, &dst, 255);
      FREESURFACE(icon);

      improvement_button = create_icon2(background, pwindow->dst,
                                        WF_FREE_THEME | WF_RESTORE_BACKGROUND);

      set_wstate(improvement_button, FC_WS_NORMAL);
      improvement_button->action = change_impr_callback;
      add_to_gui_list(MAX_ID - improvement_number(pimprove), improvement_button);

      if (impr_type_count++ >= 10) {
        set_wflag(improvement_button, WF_HIDDEN);
      }

    } improvement_iterate_end;

    FREESURFACE(background_tmpl);

    help_dlg->pEndActiveWidgetList = dock->prev;
    help_dlg->begin_widget_list = improvement_button ? improvement_button : pCloseButton;
    help_dlg->pBeginActiveWidgetList = help_dlg->begin_widget_list;

    if (impr_type_count > 10) {
      help_dlg->pActiveWidgetList = help_dlg->pEndActiveWidgetList;
      scrollbar_width = create_vertical_scrollbar(help_dlg, 1, 10, TRUE, TRUE);
    }

    /* toggle techs list button */
    pListToggleButton = create_themeicon_button_from_chars(current_theme->UP_Icon,
                                                           pwindow->dst,
                                                           _("Improvements"),
                                                           adj_font(10), 0);
#if 0
   pListToggleButton->action = toggle_full_tree_mode_in_help_dlg_callback;
   if (store->show_tree) {
      set_wstate(pListToggleButton, FC_WS_NORMAL);
   }
#endif

    widget_resize(pListToggleButton, adj_size(160), adj_size(15));
    pListToggleButton->string_utf8->fgcol = *get_theme_color(COLOR_THEME_HELPDLG_TEXT);

    add_to_gui_list(ID_BUTTON, pListToggleButton);

    dock = pListToggleButton;
    store->dock = dock;
  } else {
    created = FALSE;
    scrollbar_width = (help_dlg->pScroll ? help_dlg->pScroll->pUp_Left_Button->size.w : 0);
    pwindow = help_dlg->end_widget_list;
    store = (struct units_buttons *)pwindow->data.ptr;
    dock = store->dock;

    area = pwindow->area;

    /* delete any previous list entries */
    if (dock != help_dlg->begin_widget_list) {
      del_group_of_widgets_from_gui_list(help_dlg->begin_widget_list,
                                         dock->prev);
      help_dlg->begin_widget_list = dock;
    }
  }

  pimpr_type = improvement_by_number(impr);

  surf = get_building_surface(pimpr_type);
  pImprNameLabel = create_iconlabel_from_chars(
                     ResizeSurfaceBox(surf, adj_size(64), adj_size(48), 1, TRUE, TRUE),
                     pwindow->dst, city_improvement_name_translation(NULL, pimpr_type),
                     adj_font(24), WF_FREE_THEME);

  pImprNameLabel->ID = ID_LABEL;
  widget_add_as_prev(pImprNameLabel, dock);
  dock = pImprNameLabel;

  if (!improvement_has_flag(pimpr_type, IF_GOLD)) {
    sprintf(buffer, "%s %d", _("Base Cost:"),
            impr_base_build_shield_cost(pimpr_type));
    pCostLabel = create_iconlabel_from_chars(NULL, pwindow->dst,
                                             buffer, adj_font(12), 0);
    pCostLabel->ID = ID_LABEL;
    widget_add_as_prev(pCostLabel, dock);
    dock = pCostLabel;

    if (!is_wonder(pimpr_type)) {
      sprintf(buffer, "%s %d", _("Upkeep:"), pimpr_type->upkeep);
      pUpkeepLabel = create_iconlabel_from_chars(NULL, pwindow->dst,
                                                 buffer, adj_font(12), 0);
      pUpkeepLabel->ID = ID_LABEL;
      widget_add_as_prev(pUpkeepLabel, dock);
      dock = pUpkeepLabel;
    }
  }

  /* requirement */
  pRequirementLabel = create_iconlabel_from_chars(NULL, pwindow->dst,
                                                  _("Requirement:"),
                                                  adj_font(12), 0);
  pRequirementLabel->ID = ID_LABEL;
  widget_add_as_prev(pRequirementLabel, dock);
  dock = pRequirementLabel;

  if (requirement_vector_size(&pimpr_type->reqs) == 0) {
    pRequirementLabel2 = create_iconlabel_from_chars(NULL, pwindow->dst,
                                                     Q_("?req:None"),
                                                     adj_font(12), 0);
    pRequirementLabel2->ID = ID_LABEL;
  } else {
    /* FIXME: this should show ranges, negated reqs, and all the
     * MAX_NUM_REQS reqs.
     * Currently it's limited to 1 req. Remember MAX_NUM_REQS is a compile-time
     * definition. */
    requirement_vector_iterate(&pimpr_type->reqs, preq) {
      if (!preq->present) {
        continue;
      }
      pRequirementLabel2 = create_iconlabel_from_chars(NULL, pwindow->dst,
                             universal_name_translation(&preq->source, buffer, sizeof(buffer)),
                             adj_font(12), WF_RESTORE_BACKGROUND);
      if (preq->source.kind != VUT_ADVANCE) {
        break; /* FIXME */
      }
      pRequirementLabel2->ID = MAX_ID - advance_number(preq->source.value.advance);
      pRequirementLabel2->string_utf8->fgcol = *get_tech_color(advance_number(preq->source.value.advance));
      pRequirementLabel2->action = change_tech_callback;
      set_wstate(pRequirementLabel2, FC_WS_NORMAL);
      break;
    } requirement_vector_iterate_end;
  }
  widget_add_as_prev(pRequirementLabel2, dock);
  dock = pRequirementLabel2;
  store->pRequirementButton = pRequirementLabel2;

  /* obsolete by */
  pObsoleteByLabel = create_iconlabel_from_chars(NULL, pwindow->dst,
                                                 _("Obsolete by:"),
                                                 adj_font(12), 0);
  pObsoleteByLabel->ID = ID_LABEL;
  widget_add_as_prev(pObsoleteByLabel, dock);
  dock = pObsoleteByLabel;


  requirement_vector_iterate(&pimpr_type->obsolete_by, pobs) {
    if (pobs->source.kind == VUT_ADVANCE) {
      obsTech = pobs->source.value.advance;
      break;
    }
  } requirement_vector_iterate_end;
  if (obsTech == NULL) {
    pObsoleteByLabel2 = create_iconlabel_from_chars(NULL, pwindow->dst,
                                                    _("Never"), adj_font(12), 0);
    pObsoleteByLabel2->ID = ID_LABEL;
  } else {
    pObsoleteByLabel2 = create_iconlabel_from_chars(NULL, pwindow->dst,
                                                    advance_name_translation(obsTech),
                                                    adj_font(12), WF_RESTORE_BACKGROUND);
    pObsoleteByLabel2->ID = MAX_ID - advance_number(obsTech);
    pObsoleteByLabel2->string_utf8->fgcol = *get_tech_color(advance_number(obsTech));
    pObsoleteByLabel2->action = change_tech_callback;
    set_wstate(pObsoleteByLabel2, FC_WS_NORMAL);
  }
  widget_add_as_prev(pObsoleteByLabel2, dock);
  dock = pObsoleteByLabel2;
  store->pObsoleteByButton = pObsoleteByLabel2;

  /* helptext */
  start_x = (area.x + 1 + scrollbar_width + help_dlg->pEndActiveWidgetList->size.w + adj_size(20));

  buffer[0] = '\0';
  helptext_building(buffer, sizeof(buffer), client.conn.playing, NULL,
                    pimpr_type);
  if (buffer[0] != '\0') {
    utf8_str *bstr = create_utf8_from_char(buffer, adj_font(12));

    convert_utf8_str_to_const_surface_width(bstr, adj_size(640) - start_x - adj_size(20));
    pHelptextLabel = create_iconlabel(NULL, pwindow->dst, bstr, 0);
    pHelptextLabel->ID = ID_LABEL;
    widget_add_as_prev(pHelptextLabel, dock);
    dock = pHelptextLabel;
    text = TRUE;
  }

  help_dlg->begin_widget_list = pHelptextLabel ? pHelptextLabel : pObsoleteByLabel2;

  /* --------------------------------------------------------- */
  if (created) {

    surf = theme_get_background(theme, BACKGROUND_HELPDLG);
    if (resize_window(pwindow, surf, NULL, adj_size(640), adj_size(480))) {
      FREESURFACE(surf);
    }

    area = pwindow->area;

    widget_set_position(pwindow,
                        (main_window_width() - pwindow->size.w) / 2,
                        (main_window_height() - pwindow->size.h) / 2);

    /* exit button */
    pCloseButton = pwindow->prev;
    widget_set_position(pCloseButton,
                        area.x + area.w - pCloseButton->size.w - 1,
                        pwindow->size.y + adj_size(2));

    /* list toggle button */
    pListToggleButton = store->dock;
    widget_set_position(pListToggleButton, area.x, area.y);

    /* list entries */
    h = setup_vertical_widgets_position(1, area.x + scrollbar_width,
                                        area.y + pListToggleButton->size.h, 0, 0,
                                        help_dlg->pBeginActiveWidgetList,
                                        help_dlg->pEndActiveWidgetList);

    /* scrollbar */
    if (help_dlg->pScroll) {
      setup_vertical_scrollbar_area(help_dlg->pScroll,
                                    area.x, area.y + pListToggleButton->size.h,
                                    h, FALSE);
    }
  }

  pImprNameLabel = store->dock->prev;
  widget_set_position(pImprNameLabel, start_x, area.y + adj_size(16));

  start_y = pImprNameLabel->size.y + pImprNameLabel->size.h + adj_size(10);

  if (!improvement_has_flag(pimpr_type, IF_GOLD)) {
    pCostLabel = pImprNameLabel->prev;
    widget_set_position(pCostLabel, start_x, start_y);
    if (!is_wonder(pimpr_type)) {
      pUpkeepLabel = pCostLabel->prev;
      widget_set_position(pUpkeepLabel,
                          pCostLabel->size.x + pCostLabel->size.w + adj_size(20),
                          start_y);
    }
    start_y += pCostLabel->size.h;
  }

  pRequirementLabel = store->pRequirementButton->next;
  widget_set_position(pRequirementLabel, start_x, start_y);

  pRequirementLabel2 = store->pRequirementButton;
  widget_set_position(pRequirementLabel2,
                      pRequirementLabel->size.x + pRequirementLabel->size.w + adj_size(5),
                      start_y);

  if (store->pObsoleteByButton) {
    pObsoleteByLabel = store->pObsoleteByButton->next;
    widget_set_position(pObsoleteByLabel,
                        pRequirementLabel2->size.x + pRequirementLabel2->size.w + adj_size(10),
                        start_y);

    pObsoleteByLabel2 = store->pObsoleteByButton;
    widget_set_position(pObsoleteByLabel2,
                        pObsoleteByLabel->size.x + pObsoleteByLabel->size.w + adj_size(5),
                        start_y);

    start_y += pObsoleteByLabel2->size.h;
  }

  start_y += adj_size(30);

  if (text) {
    widget_set_position(pHelptextLabel, start_x, start_y);
  }

  redraw_impr_info_dlg();
}

/**********************************************************************//**
  User requested new unit help
**************************************************************************/
static int change_unit_callback(struct widget *pwidget)
{
  if (PRESSED_EVENT(main_data.event)) {
    popup_unit_info(MAX_ID - pwidget->ID);
  }

  return -1;
}

/**********************************************************************//**
  Refresh unit help dialog
**************************************************************************/
static void redraw_unit_info_dlg(void)
{
  SDL_Color bg_color = {255, 255, 255, 64};
  struct widget *pwindow = help_dlg->end_widget_list;
  struct units_buttons *store = (struct units_buttons *)pwindow->data.ptr;
  SDL_Rect dst;

  redraw_group(pwindow->prev, pwindow, FALSE);

  dst.x = store->dock->prev->size.x - adj_size(10);
  dst.y = store->dock->prev->size.y - adj_size(10);
  dst.w = pwindow->size.w - (dst.x - pwindow->size.x) - adj_size(10);
  dst.h = pwindow->size.h - (dst.y - pwindow->size.y) - adj_size(10);

  fill_rect_alpha(pwindow->dst->surface, &dst, &bg_color);

  create_frame(pwindow->dst->surface,
               dst.x, dst.y, dst.w, dst.h,
               get_theme_color(COLOR_THEME_HELPDLG_FRAME));

  /*------------------------------------- */
  redraw_group(help_dlg->begin_widget_list, pwindow->prev->prev, FALSE);
  widget_flush(pwindow);
}

/**********************************************************************//**
  Show improvement info
**************************************************************************/
void popup_unit_info(Unit_type_id type_id)
{
  SDL_Color bg_color = {255, 255, 255, 128};
  struct widget *pwindow;
  struct units_buttons *store;
  struct widget *pCloseButton = NULL;
  struct widget *pListToggleButton = NULL;
  struct widget *pUnitButton = NULL;
  struct widget *pUnitNameLabel = NULL;
  struct widget *pUnitInfoLabel = NULL;
  struct widget *pRequirementLabel = NULL;
  struct widget *pRequirementLabel2 = NULL;
  struct widget *pObsoleteByLabel = NULL;
  struct widget *pObsoleteByLabel2 = NULL;
  struct widget *pHelptextLabel = NULL;
  struct widget *dock;
  utf8_str *title;
  utf8_str *pstr;
  SDL_Surface *surf;
  int h, start_x, start_y, utype_count;
  bool created, text = FALSE;
  int scrollbar_width = 0;
  struct unit_type *punittype;
  char buffer[bufsz];
  SDL_Rect area;

  if (current_help_dlg != HELP_UNIT) {
    popdown_help_dialog();
  }

  /* create new dialog if it doesn't exist yet */
  if (!help_dlg) {
    SDL_Surface *background_tmpl, *background, *pText, *icon;
    SDL_Rect dst;

    current_help_dlg = HELP_UNIT;
    created = TRUE;

    /* create dialog */
    help_dlg = fc_calloc(1, sizeof(struct advanced_dialog));
    store = fc_calloc(1, sizeof(struct units_buttons));

    /* create window */
    title = create_utf8_from_char(_("Help : Units"), adj_font(12));
    title->style |= TTF_STYLE_BOLD;

    pwindow = create_window_skeleton(NULL, title, WF_FREE_DATA);
    pwindow->action = help_dlg_window_callback;
    set_wstate(pwindow , FC_WS_NORMAL);
    pwindow->data.ptr = (void *)store;
    add_to_gui_list(ID_WINDOW, pwindow);

    help_dlg->end_widget_list = pwindow;

    area = pwindow->area;

    /* ------------------ */

    /* close button */
    pCloseButton = create_themeicon(current_theme->Small_CANCEL_Icon, pwindow->dst,
                                    WF_WIDGET_HAS_INFO_LABEL
                                    | WF_RESTORE_BACKGROUND);
    pCloseButton->info_label =
        create_utf8_from_char(_("Close Dialog (Esc)"), adj_font(12));
    pCloseButton->action = exit_help_dlg_callback;
    set_wstate(pCloseButton, FC_WS_NORMAL);
    pCloseButton->key = SDLK_ESCAPE;

    add_to_gui_list(ID_BUTTON, pCloseButton);

    /* ------------------ */
    dock = pCloseButton;

    /* --- create scrollable unit list on the left side ---*/

    pstr = create_utf8_str(NULL, 0, adj_font(10));
    pstr->style |= (TTF_STYLE_BOLD | SF_CENTER);

    /* background template for entries in scroll list */
    background_tmpl = create_surf(adj_size(135), adj_size(40), SDL_SWSURFACE);
    SDL_FillRect(background_tmpl, NULL, map_rgba(background_tmpl->format, bg_color));

    create_frame(background_tmpl,
                 0, 0, background_tmpl->w - 1, background_tmpl->h - 1,
                 get_theme_color(COLOR_THEME_HELPDLG_FRAME));

    utype_count = 0;
    unit_type_iterate(ut) {

      /* copy background surface */
      background = copy_surface(background_tmpl);

      /* blit unit name */
      copy_chars_to_utf8_str(pstr, utype_name_translation(ut));
      pText = create_text_surf_smaller_than_w(pstr, adj_size(100 - 4));
      dst.x = adj_size(35) + (background->w - pText->w - adj_size(35)) / 2;
      dst.y = (background->h - pText->h) / 2;
      alphablit(pText, NULL, background, &dst, 255);
      FREESURFACE(pText);

      /* blit unit icon */
      icon = ResizeSurfaceBox(get_unittype_surface(ut, direction8_invalid()),
                              adj_size(36), adj_size(36), 1, TRUE, TRUE);
      dst.x = (adj_size(35) - icon->w) / 2;
      dst.y = (background->h - icon->h) / 2;
      alphablit(icon, NULL, background, &dst, 255);
      FREESURFACE(icon);

      pUnitButton = create_icon2(background, pwindow->dst,
                                 WF_FREE_THEME | WF_RESTORE_BACKGROUND);

      set_wstate(pUnitButton, FC_WS_NORMAL);
      pUnitButton->action = change_unit_callback;
      add_to_gui_list(MAX_ID - utype_number(ut), pUnitButton);

      if (utype_count++ >= 10) {
        set_wflag(pUnitButton, WF_HIDDEN);
      }

    } unit_type_iterate_end;

    FREESURFACE(background_tmpl);

    help_dlg->pEndActiveWidgetList = dock->prev;
    help_dlg->begin_widget_list = pUnitButton ? pUnitButton : pCloseButton;
    help_dlg->pBeginActiveWidgetList = help_dlg->begin_widget_list;

    if (utype_count > 10) {
      help_dlg->pActiveWidgetList = help_dlg->pEndActiveWidgetList;
      scrollbar_width = create_vertical_scrollbar(help_dlg, 1, 10, TRUE, TRUE);
    }

    /* toggle techs list button */
    pListToggleButton = create_themeicon_button_from_chars(current_theme->UP_Icon,
                          pwindow->dst,  _("Units"), adj_font(10), 0);
#if 0
    pListToggleButton->action = toggle_full_tree_mode_in_help_dlg_callback;
    if (store->show_tree) {
      set_wstate(pListToggleButton, FC_WS_NORMAL);
    }
#endif

    widget_resize(pListToggleButton, adj_size(160), adj_size(15));
    pListToggleButton->string_utf8->fgcol = *get_theme_color(COLOR_THEME_HELPDLG_TEXT);

    add_to_gui_list(ID_BUTTON, pListToggleButton);

    dock = pListToggleButton;
    store->dock = dock;
  } else {
    created = FALSE;
    scrollbar_width = (help_dlg->pScroll ? help_dlg->pScroll->pUp_Left_Button->size.w : 0);
    pwindow = help_dlg->end_widget_list;
    store = (struct units_buttons *)pwindow->data.ptr;
    dock = store->dock;

    area = pwindow->area;

    /* delete any previous list entries */
    if (dock != help_dlg->begin_widget_list) {
      del_group_of_widgets_from_gui_list(help_dlg->begin_widget_list,
                                         dock->prev);
      help_dlg->begin_widget_list = dock;
    }
  }

  punittype = utype_by_number(type_id);
  pUnitNameLabel= create_iconlabel_from_chars(
                adj_surf(get_unittype_surface(punittype, direction8_invalid())),
                pwindow->dst, utype_name_translation(punittype),
                adj_font(24), WF_FREE_THEME);

  pUnitNameLabel->ID = ID_LABEL;
  widget_add_as_prev(pUnitNameLabel, dock);
  dock = pUnitNameLabel;


  {
    char buf[2048];

    fc_snprintf(buf, sizeof(buf), "%s %d %s",
                _("Cost:"), utype_build_shield_cost_base(punittype),
                PL_("shield", "shields", utype_build_shield_cost_base(punittype)));

    if (punittype->pop_cost) {
      cat_snprintf(buf, sizeof(buf), " %d %s",
                   punittype->pop_cost, PL_("citizen", "citizens",
                                            punittype->pop_cost));
    }

    cat_snprintf(buf, sizeof(buf), "      %s",  _("Upkeep:"));

    if (punittype->upkeep[O_SHIELD]) {
      cat_snprintf(buf, sizeof(buf), " %d %s",
                   punittype->upkeep[O_SHIELD], PL_("shield", "shields",
                                                    punittype->upkeep[O_SHIELD]));
    }
    if (punittype->upkeep[O_FOOD]) {
      cat_snprintf(buf, sizeof(buf), " %d %s",
                   punittype->upkeep[O_FOOD], PL_("food", "foods",
                                                  punittype->upkeep[O_FOOD]));
    }
    if (punittype->upkeep[O_GOLD]) {
      cat_snprintf(buf, sizeof(buf), " %d %s",
                   punittype->upkeep[O_GOLD], PL_("gold", "golds",
                                                  punittype->upkeep[O_GOLD]));
    }
    if (punittype->happy_cost) {
      cat_snprintf(buf, sizeof(buf), " %d %s",
                   punittype->happy_cost, PL_("citizen", "citizens",
                                              punittype->happy_cost));
    }

    cat_snprintf(buf, sizeof(buf), "\n%s %d %s %d %s %s\n%s %d %s %d %s %d",
              _("Attack:"), punittype->attack_strength,
              _("Defense:"), punittype->defense_strength,
              _("Move:"), move_points_text(punittype->move_rate, TRUE),
              _("Vision:"), punittype->vision_radius_sq,
              _("FirePower:"), punittype->firepower,
              _("Hitpoints:"), punittype->hp);

    pUnitInfoLabel = create_iconlabel_from_chars(NULL, pwindow->dst, buf,
                                                 adj_font(12), 0);
    pUnitInfoLabel->ID = ID_LABEL;
    widget_add_as_prev(pUnitInfoLabel, dock);
    dock = pUnitInfoLabel;
  }

  /* requirement */
  pRequirementLabel = create_iconlabel_from_chars(NULL, pwindow->dst,
                                                  _("Requirement:"),
                                                  adj_font(12), 0);
  pRequirementLabel->ID = ID_LABEL;
  widget_add_as_prev(pRequirementLabel, dock);
  dock = pRequirementLabel;

  if (A_NEVER == punittype->require_advance
      || advance_by_number(A_NONE) == punittype->require_advance) {
    pRequirementLabel2 = create_iconlabel_from_chars(NULL, pwindow->dst,
                                                     Q_("?tech:None"), adj_font(12), 0);
    pRequirementLabel2->ID = ID_LABEL;
  } else {
    pRequirementLabel2 = create_iconlabel_from_chars(NULL, pwindow->dst,
          advance_name_translation(punittype->require_advance),
          adj_font(12),
          WF_RESTORE_BACKGROUND);
    pRequirementLabel2->ID = MAX_ID - advance_number(punittype->require_advance);
    pRequirementLabel2->string_utf8->fgcol = *get_tech_color(advance_number(punittype->require_advance));
    pRequirementLabel2->action = change_tech_callback;
    set_wstate(pRequirementLabel2, FC_WS_NORMAL);
  }
  widget_add_as_prev(pRequirementLabel2, dock);
  dock = pRequirementLabel2;
  store->pRequirementButton = pRequirementLabel2;

  /* obsolete by */
  pObsoleteByLabel = create_iconlabel_from_chars(NULL, pwindow->dst,
                                                 _("Obsolete by:"),
                                                 adj_font(12), 0);
  pObsoleteByLabel->ID = ID_LABEL;
  widget_add_as_prev(pObsoleteByLabel, dock);
  dock = pObsoleteByLabel;

  if (punittype->obsoleted_by == U_NOT_OBSOLETED) {
    pObsoleteByLabel2 = create_iconlabel_from_chars(NULL, pwindow->dst,
                                                    Q_("?utype:None"),
                                                    adj_font(12), 0);
    pObsoleteByLabel2->ID = ID_LABEL;
  } else {
    const struct unit_type *utype = punittype->obsoleted_by;

    pObsoleteByLabel2 = create_iconlabel_from_chars(NULL, pwindow->dst,
                                                    utype_name_translation(utype),
                                                    adj_font(12),
                                                    WF_RESTORE_BACKGROUND);
    pObsoleteByLabel2->string_utf8->fgcol = *get_tech_color(advance_number(utype->require_advance));
    pObsoleteByLabel2->ID = MAX_ID - utype_number(punittype->obsoleted_by);
    pObsoleteByLabel2->action = change_unit_callback;
    set_wstate(pObsoleteByLabel2, FC_WS_NORMAL);
  }
  widget_add_as_prev(pObsoleteByLabel2, dock);
  dock = pObsoleteByLabel2;
  store->pObsoleteByButton = pObsoleteByLabel2;

  /* helptext */
  start_x = (area.x + 1 + scrollbar_width + help_dlg->pActiveWidgetList->size.w + adj_size(20));

  buffer[0] = '\0';
  helptext_unit(buffer, sizeof(buffer), client.conn.playing, "", utype_by_number(type_id));
  if (buffer[0] != '\0') {
    utf8_str *ustr = create_utf8_from_char(buffer, adj_font(12));

    convert_utf8_str_to_const_surface_width(ustr, adj_size(640) - start_x - adj_size(20));
    pHelptextLabel = create_iconlabel(NULL, pwindow->dst, ustr, 0);
    pHelptextLabel->ID = ID_LABEL;
    widget_add_as_prev(pHelptextLabel, dock);
    dock = pHelptextLabel;
    text = TRUE;
  }

  help_dlg->begin_widget_list = pHelptextLabel ? pHelptextLabel : pObsoleteByLabel2;

  /* --------------------------------------------------------- */
  if (created) {

    surf = theme_get_background(theme, BACKGROUND_HELPDLG);
    if (resize_window(pwindow, surf, NULL, adj_size(640), adj_size(480))) {
      FREESURFACE(surf);
    }

    area = pwindow->area;

    widget_set_position(pwindow,
                        (main_window_width() - pwindow->size.w) / 2,
                        (main_window_height() - pwindow->size.h) / 2);

    /* exit button */
    pCloseButton = pwindow->prev;
    widget_set_position(pCloseButton,
                        area.x + area.w - pCloseButton->size.w - 1,
                        pwindow->size.y + adj_size(2));

    /* list toggle button */
    pListToggleButton = store->dock;
    widget_set_position(pListToggleButton, area.x, area.y);

    /* list entries */
    h = setup_vertical_widgets_position(1, area.x + scrollbar_width,
                                           area.y + pListToggleButton->size.h, 0, 0,
                                           help_dlg->pBeginActiveWidgetList,
                                           help_dlg->pEndActiveWidgetList);

    /* scrollbar */
    if (help_dlg->pScroll) {
      setup_vertical_scrollbar_area(help_dlg->pScroll,
                                    area.x, area.y + pListToggleButton->size.h,
                                    h, FALSE);
    }
  }

  pUnitNameLabel = store->dock->prev;
  widget_set_position(pUnitNameLabel, start_x, area.y + adj_size(16));

  start_y = pUnitNameLabel->size.y + pUnitNameLabel->size.h + adj_size(10);

  pUnitInfoLabel = pUnitNameLabel->prev;
  widget_set_position(pUnitInfoLabel, start_x, start_y);

  start_y += pUnitInfoLabel->size.h;

  pRequirementLabel = store->pRequirementButton->next;
  widget_set_position(pRequirementLabel, start_x, start_y);

  pRequirementLabel2 = store->pRequirementButton;
  widget_set_position(pRequirementLabel2,
                      pRequirementLabel->size.x + pRequirementLabel->size.w + adj_size(5),
                      start_y);

  pObsoleteByLabel = store->pObsoleteByButton->next;
  widget_set_position(pObsoleteByLabel,
                      pRequirementLabel2->size.x + pRequirementLabel2->size.w + adj_size(10),
                      start_y);

  pObsoleteByLabel2 = store->pObsoleteByButton;
  widget_set_position(pObsoleteByLabel2,
                      pObsoleteByLabel->size.x + pObsoleteByLabel->size.w + adj_size(5),
                      start_y);

  start_y += pObsoleteByLabel2->size.h + adj_size(20);

  if (text) {
    pHelptextLabel = store->pObsoleteByButton->prev;
    widget_set_position(pHelptextLabel, start_x, start_y);
  }

  redraw_unit_info_dlg();
}

/* =============================================== */
/* ==================== Tech Tree ================ */
/* =============================================== */

/**********************************************************************//**
  User requested new tech help
**************************************************************************/
static int change_tech_callback(struct widget *pwidget)
{
  if (PRESSED_EVENT(main_data.event)) {
    popup_tech_info(MAX_ID - pwidget->ID);
  }

  return -1;
}

/**********************************************************************//**
  User requested new tech tree
**************************************************************************/
static int show_tech_tree_callback(struct widget *pwidget)
{
  if (PRESSED_EVENT(main_data.event)) {
    struct techs_buttons *store = (struct techs_buttons *)help_dlg->end_widget_list->data.ptr;

    store->show_tree = !store->show_tree;
    if (!store->show_tree) {
      store->show_full_tree = FALSE;
      store->dock->theme2 = current_theme->UP_Icon;
    }
    popup_tech_info(MAX_ID - store->dock->prev->ID);
  }

  return -1;
}

/**********************************************************************//**
  Refresh tech help dialog
**************************************************************************/
static void redraw_tech_info_dlg(void)
{
  SDL_Color bg_color = {255, 255, 255, 64};
  struct widget *pwindow = help_dlg->end_widget_list;
  struct techs_buttons *store = (struct techs_buttons *)pwindow->data.ptr;
  SDL_Surface *pText0, *pText1 = NULL;
  utf8_str *pstr;
  SDL_Rect dst;

  redraw_group(pwindow->prev, pwindow, FALSE);

  dst.x = store->dock->prev->prev->size.x - adj_size(10);
  dst.y = store->dock->prev->prev->size.y - adj_size(10);
  dst.w = pwindow->size.w - (dst.x - pwindow->size.x) - adj_size(10);
  dst.h = pwindow->size.h - (dst.y - pwindow->size.y) - adj_size(10);

  fill_rect_alpha(pwindow->dst->surface, &dst, &bg_color);

  create_frame(pwindow->dst->surface,
               dst.x, dst.y, dst.w, dst.h,
               get_theme_color(COLOR_THEME_HELPDLG_FRAME));

  /* -------------------------- */
  pstr = create_utf8_from_char(_("Allows"), adj_font(14));
  pstr->style |= TTF_STYLE_BOLD;

  pText0 = create_text_surf_from_utf8(pstr);
  dst.x = store->dock->prev->prev->size.x;
  if (store->pTargets[0]) {
    dst.y = store->pTargets[0]->size.y - pText0->h;
  } else {
    dst.y = store->dock->prev->prev->size.y
              + store->dock->prev->prev->size.h + adj_size(10);
  }

  alphablit(pText0, NULL, pwindow->dst->surface, &dst, 255);
  FREESURFACE(pText0);

  if (store->pSub_Targets[0]) {
    int i;

    change_ptsize_utf8(pstr, adj_font(12));

    copy_chars_to_utf8_str(pstr, _("( with "));
    pText0 = create_text_surf_from_utf8(pstr);

    copy_chars_to_utf8_str(pstr, _(" )"));
    pText1 = create_text_surf_from_utf8(pstr);
    i = 0;
    while (i < 6 && store->pSub_Targets[i]) {
      dst.x = store->pSub_Targets[i]->size.x - pText0->w;
      dst.y = store->pSub_Targets[i]->size.y;

      alphablit(pText0, NULL, pwindow->dst->surface, &dst, 255);
      dst.x = store->pSub_Targets[i]->size.x + store->pSub_Targets[i]->size.w;
      dst.y = store->pSub_Targets[i]->size.y;

      alphablit(pText1, NULL, pwindow->dst->surface, &dst, 255);
      i++;
    }

    FREESURFACE(pText0);
    FREESURFACE(pText1);
  }
  FREEUTF8STR(pstr);

  redraw_group(help_dlg->begin_widget_list, pwindow->prev->prev, FALSE);
  widget_flush(pwindow);
}

/**********************************************************************//**
  Create tech info widgets
**************************************************************************/
static struct widget *create_tech_info(Tech_type_id tech, int width,
                                       struct widget *pwindow,
                                       struct techs_buttons *store)
{
  struct widget *pwidget;
  struct widget *pLast, *pBudynki;
  struct widget *dock = store->dock;
  int i, targets_count,sub_targets_count, max_width = 0;
  int start_x, start_y, imp_count, unit_count, flags_count, gov_count;
  char buffer[bufsz];
  SDL_Surface *surf;

  start_x = (pwindow->area.x + adj_size(1) + width + help_dlg->pActiveWidgetList->size.w + adj_size(20));

  /* tech tree icon */
  pwidget = create_icon2(current_theme->Tech_Tree_Icon, pwindow->dst,
                         WF_RESTORE_BACKGROUND);

  set_wstate(pwidget, FC_WS_NORMAL);
  pwidget->action = show_tech_tree_callback;
  pwidget->ID = MAX_ID - tech;
  widget_add_as_prev(pwidget, dock);
  dock = pwidget;

  /* tech name (heading) */
  pwidget = create_iconlabel_from_chars(get_tech_icon(tech),
                    pwindow->dst,
                    advance_name_translation(advance_by_number(tech)),
                    adj_font(24),
                    WF_FREE_THEME);

  pwidget->ID = ID_LABEL;
  widget_add_as_prev(pwidget, dock);
  dock = pwidget;

  /* target techs */
  targets_count = 0;
  advance_index_iterate(A_FIRST, aidx) {
    if ((targets_count < 6)
        && (advance_required(aidx, AR_ONE) == tech
            || advance_required(aidx, AR_TWO) == tech)) {
      pwidget = create_iconlabel_from_chars(NULL, pwindow->dst,
              advance_name_translation(advance_by_number(aidx)),
              adj_font(12),
              WF_RESTORE_BACKGROUND);
      pwidget->string_utf8->fgcol = *get_tech_color(aidx);
      max_width = MAX(max_width, pwidget->size.w);
      set_wstate(pwidget, FC_WS_NORMAL);
      pwidget->action = change_tech_callback;
      pwidget->ID = MAX_ID - aidx;
      widget_add_as_prev(pwidget, dock);
      dock = pwidget;
      store->pTargets[targets_count++] = pwidget;
    }
  } advance_index_iterate_end;
  if (targets_count < 6) {
    store->pTargets[targets_count] = NULL;
  }

  sub_targets_count = 0;
  if (targets_count > 0) {
    int sub_tech;

    for (i = 0; i < targets_count; i++) {
      sub_tech = MAX_ID - store->pTargets[i]->ID;
      if (advance_required(sub_tech, AR_ONE) == tech
          && advance_required(sub_tech, AR_TWO) != A_NONE) {
        sub_tech = advance_required(sub_tech, AR_TWO);
      } else if (advance_required(sub_tech, AR_TWO) == tech
                 && advance_required(sub_tech, AR_ONE) != A_NONE) {
        sub_tech = advance_required(sub_tech, AR_ONE);
      } else {
        continue;
      }
      pwidget = create_iconlabel_from_chars(NULL, pwindow->dst,
              advance_name_translation(advance_by_number(sub_tech)),
              adj_font(12),
              WF_RESTORE_BACKGROUND);
      pwidget->string_utf8->fgcol = *get_tech_color(sub_tech);
      set_wstate(pwidget, FC_WS_NORMAL);
      pwidget->action = change_tech_callback;
      pwidget->ID = MAX_ID - sub_tech;
      widget_add_as_prev(pwidget, dock);
      dock = pwidget;
      store->pSub_Targets[sub_targets_count++] = pwidget;
    }
  }
  if (sub_targets_count < 6) {
    store->pSub_Targets[sub_targets_count] = NULL;
  }

  /* fill array with iprvm. icons */
  pBudynki = pwidget;

  /* target governments */
  gov_count = 0;
  governments_iterate(gov) {
    requirement_vector_iterate(&(gov->reqs), preq) {
      if (VUT_ADVANCE == preq->source.kind
          && advance_number(preq->source.value.advance) == tech) {

        pwidget = create_iconlabel_from_chars(adj_surf(get_government_surface(gov)),
                pwindow->dst,
                government_name_translation(gov),
                adj_font(14),
                WF_RESTORE_BACKGROUND | WF_SELECT_WITHOUT_BAR | WF_FREE_THEME);
        set_wstate(pwidget, FC_WS_NORMAL);
        pwidget->action = change_gov_callback;
        pwidget->ID = MAX_ID - government_index(gov);
        widget_add_as_prev(pwidget, dock);
        dock = pwidget;
        gov_count++;
      }
    } requirement_vector_iterate_end;
  } governments_iterate_end;

  /* target improvements */
  imp_count = 0;
  improvement_iterate(pimprove) {
    /* FIXME: this should show ranges and all the MAX_NUM_REQS reqs.
     * Currently it's limited to 1 req. Remember MAX_NUM_REQS is a compile-time
     * definition. */
    requirement_vector_iterate(&(pimprove->reqs), preq) {
      if (VUT_ADVANCE == preq->source.kind
          && advance_number(preq->source.value.advance) == tech) {
        surf = get_building_surface(pimprove);
        pwidget = create_iconlabel_from_chars(
                ResizeSurfaceBox(surf, adj_size(48), adj_size(48), 1, TRUE, TRUE),
                pwindow->dst,
                improvement_name_translation(pimprove),
                adj_font(14),
                WF_RESTORE_BACKGROUND | WF_SELECT_WITHOUT_BAR);
        set_wstate(pwidget, FC_WS_NORMAL);
        if (is_wonder(pimprove)) {
          pwidget->string_utf8->fgcol = *get_theme_color(COLOR_THEME_CITYDLG_LUX);
        }
        pwidget->action = change_impr_callback;
        pwidget->ID = MAX_ID - improvement_number(pimprove);
        widget_add_as_prev(pwidget, dock);
        dock = pwidget;
        imp_count++;
      }

      break;
    } requirement_vector_iterate_end;
  } improvement_iterate_end;

  unit_count = 0;
  unit_type_iterate(un) {
    struct unit_type *punittype = un;

    if (advance_number(punittype->require_advance) == tech) {
      pwidget = create_iconlabel_from_chars(
                                   ResizeSurfaceBox(get_unittype_surface(un, direction8_invalid()),
                                   adj_size(48), adj_size(48), 1, TRUE, TRUE),
                  pwindow->dst, utype_name_translation(punittype), adj_font(14),
                  (WF_FREE_THEME | WF_RESTORE_BACKGROUND | WF_SELECT_WITHOUT_BAR));
      set_wstate(pwidget, FC_WS_NORMAL);
      pwidget->action = change_unit_callback;
      pwidget->ID = MAX_ID - utype_number(un);
      widget_add_as_prev(pwidget, dock);
      dock = pwidget;
      unit_count++;
    }
  } unit_type_iterate_end;

  buffer[0] = '\0';
  if (tech != A_NONE) {
    helptext_advance(buffer, sizeof(buffer), client.conn.playing, "", tech);
  }
  if (buffer[0] != '\0') {
    utf8_str *pstr = create_utf8_from_char(buffer, adj_font(12));

    convert_utf8_str_to_const_surface_width(pstr, adj_size(640) - start_x - adj_size(20));
    pwidget = create_iconlabel(NULL, pwindow->dst, pstr, 0);
    pwidget->ID = ID_LABEL;
    widget_add_as_prev(pwidget, dock);
    dock = pwidget;
    flags_count = 1;
  } else {
    flags_count = 0;
  }

  pLast = pwidget;
  /* --------------------------------------------- */

  /* tree button */
  pwidget = store->dock->prev;
  pwidget->size.x = pwindow->area.x + pwindow->area.w - pwidget->size.w - adj_size(17);
  pwidget->size.y = pwindow->area.y + adj_size(16);

  /* Tech label */
  pwidget = pwidget->prev;
  pwidget->size.x = start_x;
  pwidget->size.y = pwindow->area.y + adj_size(16);
  start_y = pwidget->size.y + pwidget->size.h + adj_size(30);

  if (targets_count) {
    int j, t0, t1;

    i = 0;
    j = 0;
    t1 = MAX_ID - store->pSub_Targets[j]->ID;
    while (i < 6 && store->pTargets[i]) {
      store->pTargets[i]->size.x = pwindow->size.x + start_x;
      store->pTargets[i]->size.y = start_y;

      if (store->pSub_Targets[j]) {
        t0 = MAX_ID - store->pTargets[i]->ID;
        t1 = MAX_ID - store->pSub_Targets[j]->ID;
        if (advance_required(t0, AR_ONE) == t1
            || advance_required(t0, AR_TWO) == t1) {
          store->pSub_Targets[j]->size.x = pwindow->size.x + start_x + max_width + 60;
          store->pSub_Targets[j]->size.y = store->pTargets[i]->size.y;
          j++;
        }
      }

      start_y += store->pTargets[i]->size.h;
      i++;
    }

    start_y += adj_size(10);
  }
  pwidget = NULL;

  if (gov_count) {
    pwidget = pBudynki->prev;
    while (gov_count-- && pwidget) {
      pwidget->size.x = pwindow->size.x + start_x;
      pwidget->size.y = start_y;
      start_y += pwidget->size.h + adj_size(2);
      pwidget = pwidget->prev;
    }
  }

  if (imp_count) {
    if (!pwidget) {
      pwidget = pBudynki->prev;
    }
    while (imp_count-- && pwidget) {
      pwidget->size.x = pwindow->size.x + start_x;
      pwidget->size.y = start_y;
      start_y += pwidget->size.h + adj_size(2);
      pwidget = pwidget->prev;
    }
  }

  if (unit_count) {
    if (!pwidget) {
      pwidget = pBudynki->prev;
    }
    while (unit_count-- && pwidget) {
      pwidget->size.x = pwindow->size.x + start_x;
      pwidget->size.y = start_y;
      start_y += pwidget->size.h + adj_size(2);
      pwidget = pwidget->prev;
    }
  }

  if (flags_count) {
    if (!pwidget) {
      pwidget = pBudynki->prev;
    }
    while (flags_count-- && pwidget) {
      pwidget->size.x = pwindow->size.x + start_x;
      pwidget->size.y = start_y;
      start_y += pwidget->size.h + adj_size(2);
      pwidget = pwidget->prev;
    }
  }

  return pLast;
}

/**********************************************************************//**
  Refresh tech tree dialog
**************************************************************************/
static void redraw_tech_tree_dlg(void)
{
  SDL_Color *line_color = get_theme_color(COLOR_THEME_HELPDLG_LINE);
  SDL_Color bg_color = {255, 255, 255, 64};
  struct widget *pwindow = help_dlg->end_widget_list;
  struct widget *pSub0, *pSub1;
  struct techs_buttons *store = (struct techs_buttons *)pwindow->data.ptr;
  struct widget *ptech = store->dock->prev;
  int i,j, tech, count;
  int step;
  int mod;
  SDL_Rect dst;

  /* Redraw Window with exit button */
  redraw_group(pwindow->prev, pwindow, FALSE);

  dst.x = pwindow->area.x + pwindow->area.w - adj_size(459) - adj_size(7);
  dst.y = pwindow->area.y + adj_size(6);
  dst.w = pwindow->area.w - (dst.x - pwindow->area.x) - adj_size(10);
  dst.h = pwindow->area.h - (dst.y - pwindow->area.y) - adj_size(10);

  fill_rect_alpha(pwindow->dst->surface, &dst, &bg_color);

  create_frame(pwindow->dst->surface,
               dst.x, dst.y, dst.w, dst.h,
               get_theme_color(COLOR_THEME_HELPDLG_FRAME));

  /* Draw Req arrows */
  i = 0;
  while (i < 4 && store->pSub_Req[i]) {
    i++;
  }
  count = i;

  i = 0;
  while (i < 2 && store->pRequirementButton[i]) {
    tech = MAX_ID - store->pRequirementButton[i]->ID;

    /*find Sub_Req's */
    if (i) {
      pSub0 = NULL;
      for (j = count - 1; j >= 0; j--) {
        if (MAX_ID - store->pSub_Req[j]->ID == advance_required(tech, AR_ONE)) {
          pSub0 = store->pSub_Req[j];
          break;
        }
      }

      pSub1 = NULL;
      for (j = count - 1; j >= 0; j--) {
        if (MAX_ID - store->pSub_Req[j]->ID == advance_required(tech, AR_TWO)) {
          pSub1 = store->pSub_Req[j];
          break;
        }
      }
    } else {
      pSub0 = NULL;
      for (j = 0; j < 4 && store->pSub_Req[j]; j++) {
        if (MAX_ID - store->pSub_Req[j]->ID == advance_required(tech, AR_ONE)) {
          pSub0 = store->pSub_Req[j];
          break;
        }
      }

      pSub1 = NULL;
      for (j = 0; j < 4 && store->pSub_Req[j]; j++) {
        if (MAX_ID - store->pSub_Req[j]->ID == advance_required(tech, AR_TWO)) {
          pSub1 = store->pSub_Req[j];
          break;
        }
      }
    }

    /* draw main Arrow */
    create_line(store->pRequirementButton[i]->dst->surface,
           store->pRequirementButton[i]->size.x + store->pRequirementButton[i]->size.w,
           store->pRequirementButton[i]->size.y + store->pRequirementButton[i]->size.h / 2,
           ptech->size.x,
           store->pRequirementButton[i]->size.y + store->pRequirementButton[i]->size.h / 2,
           line_color);

    /* Draw Sub_Req arrows */
    if (pSub0 || pSub1) {
      create_line(store->pRequirementButton[i]->dst->surface,
             store->pRequirementButton[i]->size.x - adj_size(10),
             store->pRequirementButton[i]->size.y + store->pRequirementButton[i]->size.h / 2,
             store->pRequirementButton[i]->size.x ,
             store->pRequirementButton[i]->size.y + store->pRequirementButton[i]->size.h / 2,
             line_color);
    }

    if (pSub0) {
      create_line(store->pRequirementButton[i]->dst->surface,
             store->pRequirementButton[i]->size.x - adj_size(10),
             pSub0->size.y + pSub0->size.h / 2,
             store->pRequirementButton[i]->size.x - adj_size(10),
             store->pRequirementButton[i]->size.y + store->pRequirementButton[i]->size.h / 2,
             line_color);
      create_line(store->pRequirementButton[i]->dst->surface,
             pSub0->size.x + pSub0->size.w,
             pSub0->size.y + pSub0->size.h / 2,
             store->pRequirementButton[i]->size.x - adj_size(10),
             pSub0->size.y + pSub0->size.h / 2,
             line_color);
    }

    if (pSub1) {
      create_line(store->pRequirementButton[i]->dst->surface,
             store->pRequirementButton[i]->size.x - adj_size(10),
             pSub1->size.y + pSub1->size.h / 2,
             store->pRequirementButton[i]->size.x - adj_size(10),
             store->pRequirementButton[i]->size.y + store->pRequirementButton[i]->size.h / 2,
             line_color);
      create_line(store->pRequirementButton[i]->dst->surface,
             pSub1->size.x + pSub1->size.w,
             pSub1->size.y + pSub1->size.h / 2,
             store->pRequirementButton[i]->size.x - adj_size(10),
             pSub1->size.y + pSub1->size.h / 2,
             line_color);
    }
    i++;
  }

  i = 0;
  while (i < 6 && store->pTargets[i]) {
    i++;
  }
  count = i;

  if (count > 4) {
    mod = 3;
  } else {
    mod = 2;
  }

  for (i = 0; i < count; i++) {
    tech = MAX_ID - store->pTargets[i]->ID;
    step = ptech->size.h / (count + 1);

    switch ((i % mod)) {
    case 2:
      line_color = get_theme_color(COLOR_THEME_HELPDLG_LINE2);
      break;
    case 1:
      line_color = get_theme_color(COLOR_THEME_HELPDLG_LINE3);
      break;
    default:
      line_color = get_theme_color(COLOR_THEME_HELPDLG_LINE);
      break;
    }

    /* find Sub_Req's */
    if (advance_required(tech, AR_ONE) == MAX_ID - ptech->ID) {
      pSub0 = ptech;
    } else {
      pSub0 = NULL;
      for (j = 0; j < 6 && store->pSub_Targets[j]; j++) {
        if (MAX_ID - store->pSub_Targets[j]->ID == advance_required(tech, AR_ONE)) {
          pSub0 = store->pSub_Targets[j];
          break;
        }
      }
    }

    if (advance_required(tech, AR_TWO) == MAX_ID - ptech->ID) {
      pSub1 = ptech;
    } else {
      pSub1 = NULL;
      for (j = 0; j < 6 && store->pSub_Targets[j]; j++) {
        if (MAX_ID - store->pSub_Targets[j]->ID == advance_required(tech, AR_TWO)) {
          pSub1 = store->pSub_Targets[j];
          break;
        }
      }
    }

    /* Draw Sub_Targets arrows */
    if (pSub0 || pSub1) {
      create_line(store->pTargets[i]->dst->surface,
                  store->pTargets[i]->size.x - ((i % mod) + 1) * 6,
                  store->pTargets[i]->size.y + store->pTargets[i]->size.h / 2,
                  store->pTargets[i]->size.x ,
                  store->pTargets[i]->size.y + store->pTargets[i]->size.h / 2,
                  line_color);
    }

    if (pSub0) {
      int y;

      if (pSub0 == ptech) {
        y = pSub0->size.y + step * (i + 1);
      } else {
        y = pSub0->size.y + pSub0->size.h / 2;
      }

      create_line(store->pTargets[i]->dst->surface,
                  store->pTargets[i]->size.x - ((i % mod) + 1) * 6,
                  y,
                  store->pTargets[i]->size.x - ((i % mod) + 1) * 6,
                  store->pTargets[i]->size.y + store->pTargets[i]->size.h / 2,
                  line_color);
      create_line(store->pTargets[i]->dst->surface,
                  pSub0->size.x + pSub0->size.w,
                  y,
                  store->pTargets[i]->size.x - ((i % mod) + 1) * 6,
                  y,
                  line_color);
    }

    if (pSub1) {
      int y;

      if (pSub1 == ptech) {
        y = pSub1->size.y + step * (i + 1);
      } else {
        y = pSub1->size.y + pSub1->size.h / 2;
      }

      create_line(store->pTargets[i]->dst->surface,
                  store->pTargets[i]->size.x - ((i % mod) + 1) * 6,
                  y,
                  store->pTargets[i]->size.x - ((i % mod) + 1) * 6,
                  store->pTargets[i]->size.y + store->pTargets[i]->size.h / 2,
                  line_color);
      create_line(store->pTargets[i]->dst->surface,
                  pSub1->size.x + pSub1->size.w,
                  y,
                  store->pTargets[i]->size.x - ((i % mod) + 1) * 6,
                  y,
                  line_color);
    }
  }

  /* Redraw rest */
  redraw_group(help_dlg->begin_widget_list, pwindow->prev->prev, FALSE);

  widget_flush(pwindow);
}

/**********************************************************************//**
  User requested toggling between full tech tree and single tech
**************************************************************************/
static int toggle_full_tree_mode_in_help_dlg_callback(struct widget *pwidget)
{
  if (PRESSED_EVENT(main_data.event)) {
    struct techs_buttons *store = (struct techs_buttons *)help_dlg->end_widget_list->data.ptr;

    if (store->show_full_tree) {
      pwidget->theme2 = current_theme->UP_Icon;
    } else {
      pwidget->theme2 = current_theme->DOWN_Icon;
    }
    store->show_full_tree = !store->show_full_tree;
    popup_tech_info(MAX_ID - store->dock->prev->ID);
  }

  return -1;
}

/**********************************************************************//**
  Create tech tree widgets
**************************************************************************/
static struct widget *create_tech_tree(Tech_type_id tech, int width,
                                       struct widget *pwindow,
                                       struct techs_buttons *store)
{
  int i, w, h, req_count , targets_count, sub_req_count, sub_targets_count;
  struct widget *pwidget;
  struct widget *ptech;
  utf8_str *pstr;
  SDL_Surface *surf;
  struct widget *dock = store->dock;

  pstr = create_utf8_str(NULL, 0, adj_font(10));
  pstr->style |= (TTF_STYLE_BOLD | SF_CENTER);

  copy_chars_to_utf8_str(pstr, advance_name_translation(advance_by_number(tech)));
  surf = create_select_tech_icon(pstr, tech, FULL_MODE);
  pwidget = create_icon2(surf, pwindow->dst,
                WF_FREE_THEME | WF_RESTORE_BACKGROUND);

  set_wstate(pwidget, FC_WS_NORMAL);
  pwidget->action = show_tech_tree_callback;
  pwidget->ID = MAX_ID - tech;
  widget_add_as_prev(pwidget, dock);
  ptech = pwidget;
  dock = pwidget;

  req_count  = 0;
  for (i = AR_ONE; i <= AR_TWO; i++) {
    Tech_type_id ar = advance_required(tech, i);
    struct advance *vap = valid_advance_by_number(ar);

    if (NULL != vap && A_NONE != ar) {
      copy_chars_to_utf8_str(pstr, advance_name_translation(vap));
      surf = create_select_tech_icon(pstr, ar, SMALL_MODE);
      pwidget = create_icon2(surf, pwindow->dst,
                WF_FREE_THEME | WF_RESTORE_BACKGROUND);
      set_wstate(pwidget, FC_WS_NORMAL);
      pwidget->action = change_tech_callback;
      pwidget->ID = MAX_ID - ar;
      widget_add_as_prev(pwidget, dock);
      dock = pwidget;
      store->pRequirementButton[i] = pwidget;
      req_count++;
    } else {
      store->pRequirementButton[i] = NULL;
    }
  }

  sub_req_count = 0;

  if (store->show_full_tree && req_count) {
    int j, sub_tech;

    for (j = 0; j < req_count; j++) {
      sub_tech = MAX_ID - store->pRequirementButton[j]->ID;
      for (i = AR_ONE; i <= AR_TWO; i++) {
        Tech_type_id ar = advance_required(sub_tech, i);
        struct advance *vap = valid_advance_by_number(ar);

        if (NULL != vap && A_NONE != ar) {
          copy_chars_to_utf8_str(pstr, advance_name_translation(vap));
          surf = create_select_tech_icon(pstr, ar, SMALL_MODE);
          pwidget = create_icon2(surf, pwindow->dst,
                WF_FREE_THEME | WF_RESTORE_BACKGROUND);
          set_wstate(pwidget, FC_WS_NORMAL);
          pwidget->action = change_tech_callback;
          pwidget->ID = MAX_ID - ar;
          widget_add_as_prev(pwidget, dock);
          dock = pwidget;
          store->pSub_Req[sub_req_count++] = pwidget;
        }
      }
    }
  }

  if (sub_req_count < 4) {
    store->pSub_Req[sub_req_count] = NULL;
  }

  targets_count = 0;
  advance_index_iterate(A_FIRST, aidx) {
    if ((targets_count < 6)
        && (advance_required(aidx, AR_ONE) == tech
            || advance_required(aidx, AR_TWO) == tech)) {
      copy_chars_to_utf8_str(pstr, advance_name_translation(advance_by_number(aidx)));
      surf = create_select_tech_icon(pstr, aidx, SMALL_MODE);
      pwidget = create_icon2(surf, pwindow->dst,
                WF_FREE_THEME | WF_RESTORE_BACKGROUND);

      set_wstate(pwidget, FC_WS_NORMAL);
      pwidget->action = change_tech_callback;
      pwidget->ID = MAX_ID - aidx;
      widget_add_as_prev(pwidget, dock);
      dock = pwidget;
      store->pTargets[targets_count++] = pwidget;
    }
  } advance_index_iterate_end;
  if (targets_count < 6) {
    store->pTargets[targets_count] = NULL;
  }

  sub_targets_count = 0;
  if (targets_count) {
    int sub_tech;

    for (i = 0; i < targets_count; i++) {
      sub_tech = MAX_ID - store->pTargets[i]->ID;
      if (advance_required(sub_tech, AR_ONE) == tech
          && advance_required(sub_tech, AR_TWO) != A_NONE) {
        sub_tech = advance_required(sub_tech, AR_TWO);
      } else if (advance_required(sub_tech, AR_TWO) == tech
                 && advance_required(sub_tech, AR_ONE) != A_NONE) {
        sub_tech = advance_required(sub_tech, AR_ONE);
      } else {
        continue;
      }

      copy_chars_to_utf8_str(pstr, advance_name_translation(advance_by_number(sub_tech)));
      surf = create_select_tech_icon(pstr, sub_tech, SMALL_MODE);
      pwidget = create_icon2(surf, pwindow->dst,
        WF_FREE_THEME | WF_RESTORE_BACKGROUND);
      set_wstate(pwidget, FC_WS_NORMAL);
      pwidget->action = change_tech_callback;
      pwidget->ID = MAX_ID - sub_tech;
      widget_add_as_prev(pwidget, dock);
      dock = pwidget;
      store->pSub_Targets[sub_targets_count++] = pwidget;
    }
  }
  if (sub_targets_count < 6) {
    store->pSub_Targets[sub_targets_count] = NULL;
  }

  FREEUTF8STR(pstr);

  /* ------------------------------------------ */
  if (sub_req_count) {
    w = (adj_size(20) + store->pSub_Req[0]->size.w) * 2;
    w += (pwindow->size.w - (20 + store->pSub_Req[0]->size.w + w + ptech->size.w)) / 2;
  } else {
    if (req_count) {
      w = (pwindow->area.x + 1 + width + store->pRequirementButton[0]->size.w * 2 + adj_size(20));
      w += (pwindow->size.w - ((adj_size(20) + store->pRequirementButton[0]->size.w) + w + ptech->size.w)) / 2;
    } else {
      w = (pwindow->size.w - ptech->size.w) / 2;
    }
  }

  ptech->size.x = pwindow->size.x + w;
  ptech->size.y = pwindow->area.y + (pwindow->area.h - ptech->size.h) / 2;

  if (req_count) {
    h = (req_count == 1 ? store->pRequirementButton[0]->size.h :
        req_count * (store->pRequirementButton[0]->size.h + adj_size(80)) - adj_size(80));
    h = ptech->size.y + (ptech->size.h - h) / 2;
    for (i = 0; i < req_count; i++) {
      store->pRequirementButton[i]->size.x = ptech->size.x - adj_size(20) - store->pRequirementButton[i]->size.w;
      store->pRequirementButton[i]->size.y = h;
      h += (store->pRequirementButton[i]->size.h + adj_size(80));
    }
  }

  if (sub_req_count) {
    h = (sub_req_count == 1 ? store->pSub_Req[0]->size.h :
     sub_req_count * (store->pSub_Req[0]->size.h + adj_size(20)) - adj_size(20));
    h = ptech->size.y + (ptech->size.h - h) / 2;
    for (i = 0; i < sub_req_count; i++) {
      store->pSub_Req[i]->size.x = ptech->size.x - (adj_size(20) + store->pSub_Req[i]->size.w) * 2;
      store->pSub_Req[i]->size.y = h;
      h += (store->pSub_Req[i]->size.h + adj_size(20));
    }
  }

  if (targets_count) {
    h = (targets_count == 1 ? store->pTargets[0]->size.h :
     targets_count * (store->pTargets[0]->size.h + adj_size(20)) - adj_size(20));
    h = ptech->size.y + (ptech->size.h - h) / 2;
    for (i = 0; i < targets_count; i++) {
      store->pTargets[i]->size.x = ptech->size.x + ptech->size.w + adj_size(20);
      store->pTargets[i]->size.y = h;
      h += (store->pTargets[i]->size.h + adj_size(20));
    }
  }

  if (sub_targets_count) {
    if (sub_targets_count < 3) {
      store->pSub_Targets[0]->size.x = ptech->size.x + ptech->size.w - store->pSub_Targets[0]->size.w;
      store->pSub_Targets[0]->size.y = ptech->size.y - store->pSub_Targets[0]->size.h - adj_size(10);
      if (store->pSub_Targets[1]) {
        store->pSub_Targets[1]->size.x = ptech->size.x + ptech->size.w - store->pSub_Targets[1]->size.w;
        store->pSub_Targets[1]->size.y = ptech->size.y + ptech->size.h + adj_size(10);
      }
    } else {
      if (sub_targets_count < 5) {
        for (i = 0; i < MIN(sub_targets_count, 4); i++) {
          store->pSub_Targets[i]->size.x = ptech->size.x + ptech->size.w - store->pSub_Targets[i]->size.w;
          if (i < 2) {
            store->pSub_Targets[i]->size.y = ptech->size.y - (store->pSub_Targets[i]->size.h + adj_size(5)) * ( 2 - i );
          } else {
            store->pSub_Targets[i]->size.y = ptech->size.y + ptech->size.h + adj_size(5)  + (store->pSub_Targets[i]->size.h + adj_size(5)) * ( i - 2 );
          }
        }
      } else {
        h = (store->pSub_Targets[0]->size.h + adj_size(6));
        for (i = 0; i < MIN(sub_targets_count, 6); i++) {
          switch (i) {
          case 0:
            store->pSub_Targets[i]->size.x = ptech->size.x + ptech->size.w - store->pSub_Targets[i]->size.w;
            store->pSub_Targets[i]->size.y = ptech->size.y - h * 2;
            break;
          case 1:
            store->pSub_Targets[i]->size.x = ptech->size.x + ptech->size.w - store->pSub_Targets[i]->size.w * 2 - adj_size(10);
            store->pSub_Targets[i]->size.y = ptech->size.y - h - h / 2;
            break;
          case 2:
            store->pSub_Targets[i]->size.x = ptech->size.x + ptech->size.w - store->pSub_Targets[i]->size.w;
            store->pSub_Targets[i]->size.y = ptech->size.y - h;
            break;
          case 3:
            store->pSub_Targets[i]->size.x = ptech->size.x + ptech->size.w - store->pSub_Targets[i]->size.w;
            store->pSub_Targets[i]->size.y = ptech->size.y + ptech->size.h + adj_size(6);
            break;
          case 4:
            store->pSub_Targets[i]->size.x = ptech->size.x + ptech->size.w - store->pSub_Targets[i]->size.w;
            store->pSub_Targets[i]->size.y = ptech->size.y + ptech->size.h + adj_size(6) + h;
            break;
          default:
            store->pSub_Targets[i]->size.x = ptech->size.x + ptech->size.w - store->pSub_Targets[i]->size.w * 2 - adj_size(10);
            store->pSub_Targets[i]->size.y = ptech->size.y + ptech->size.h + adj_size(6) + h / 2 ;
            break;
          }
        }
      }
    }
  }

  return pwidget;
}

/**********************************************************************//**
  Show tech info
**************************************************************************/
void popup_tech_info(Tech_type_id tech)
{
  struct widget *pwindow;
  struct techs_buttons *store;
  struct widget *pCloseButton = NULL;
  struct widget *pAdvanceLabel = NULL;
  struct widget *pListToggleButton = NULL;
  struct widget *dock;
  utf8_str *title;
  utf8_str *pstr;
  SDL_Surface *surf;
  int h, tech_count;
  bool created;
  int scrollbar_width = 0;
  SDL_Rect area;

  if (current_help_dlg != HELP_TECH) {
    popdown_help_dialog();
  }

  /* create new dialog if it doesn't exist yet */
  if (!help_dlg) {
    current_help_dlg = HELP_TECH;
    created = TRUE;

    /* create dialog */
    help_dlg = fc_calloc(1, sizeof(struct advanced_dialog));
    store = fc_calloc(1, sizeof(struct techs_buttons));

    store->show_tree = FALSE;
    store->show_full_tree = FALSE;

    /* create window */
    title = create_utf8_from_char(_("Help : Advances Tree"), adj_font(12));
    title->style |= TTF_STYLE_BOLD;

    pwindow = create_window_skeleton(NULL, title, WF_FREE_DATA);
    pwindow->data.ptr = (void *)store;
    pwindow->action = help_dlg_window_callback;
    set_wstate(pwindow , FC_WS_NORMAL);

    add_to_gui_list(ID_WINDOW, pwindow);

    help_dlg->end_widget_list = pwindow;

    area = pwindow->area;

    /* ------------------ */

    /* close button */
    pCloseButton = create_themeicon(current_theme->Small_CANCEL_Icon, pwindow->dst,
                                    WF_WIDGET_HAS_INFO_LABEL
                                    | WF_RESTORE_BACKGROUND);
    pCloseButton->info_label =
        create_utf8_from_char(_("Close Dialog (Esc)"), adj_font(12));
    pCloseButton->action = exit_help_dlg_callback;
    set_wstate(pCloseButton, FC_WS_NORMAL);
    pCloseButton->key = SDLK_ESCAPE;

    add_to_gui_list(ID_BUTTON, pCloseButton);

    /* ------------------ */
    dock = pCloseButton;

    /* --- create scrollable advance list on the left side ---*/
    pstr = create_utf8_str(NULL, 0, adj_font(10));
    pstr->style |= (TTF_STYLE_BOLD | SF_CENTER);

    tech_count = 0;
    advance_index_iterate(A_FIRST, i) {
      struct advance *vap = valid_advance_by_number(i);

      if (vap) {
        copy_chars_to_utf8_str(pstr, advance_name_translation(vap));
        surf = create_select_tech_icon(pstr, i, SMALL_MODE);
        pAdvanceLabel = create_icon2(surf, pwindow->dst,
                                     WF_FREE_THEME | WF_RESTORE_BACKGROUND);

        set_wstate(pAdvanceLabel, FC_WS_NORMAL);
        pAdvanceLabel->action = change_tech_callback;
        add_to_gui_list(MAX_ID - i, pAdvanceLabel);

        if (tech_count++ >= 10) {
          set_wflag(pAdvanceLabel, WF_HIDDEN);
        }
      }
    } advance_index_iterate_end;

    FREEUTF8STR(pstr);

    help_dlg->pEndActiveWidgetList = dock->prev;
    help_dlg->begin_widget_list = pAdvanceLabel ? pAdvanceLabel : pCloseButton;
    help_dlg->pBeginActiveWidgetList = help_dlg->begin_widget_list;

    if (tech_count > 10) {
      help_dlg->pActiveWidgetList = help_dlg->pEndActiveWidgetList;
      scrollbar_width = create_vertical_scrollbar(help_dlg, 1, 10, TRUE, TRUE);
    }

    /* toggle techs list button */
    pListToggleButton = create_themeicon_button_from_chars(current_theme->UP_Icon,
                                                           pwindow->dst,
                                                           _("Advances"),
                                                           adj_font(10), 0);
    pListToggleButton->action = toggle_full_tree_mode_in_help_dlg_callback;
    if (store->show_tree) {
      set_wstate(pListToggleButton, FC_WS_NORMAL);
    }
    widget_resize(pListToggleButton, adj_size(160), adj_size(15));
    pListToggleButton->string_utf8->fgcol = *get_theme_color(COLOR_THEME_HELPDLG_TEXT);

    add_to_gui_list(ID_BUTTON, pListToggleButton);

    dock = pListToggleButton;
    store->dock = dock;
  } else {
    created = FALSE;
    scrollbar_width = (help_dlg->pScroll ? help_dlg->pScroll->pUp_Left_Button->size.w: 0);
    pwindow = help_dlg->end_widget_list;
    store = (struct techs_buttons *)pwindow->data.ptr;
    dock = store->dock;

    area = pwindow->area;

    /* delete any previous list entries */
    if (dock != help_dlg->begin_widget_list) {
      del_group_of_widgets_from_gui_list(help_dlg->begin_widget_list, dock->prev);
      help_dlg->begin_widget_list = dock;
    }

    /* show/hide techs list */
    pListToggleButton = dock;

    if (store->show_tree) {
      set_wstate(pListToggleButton, FC_WS_NORMAL);
    } else {
      set_wstate(pListToggleButton, FC_WS_DISABLED);
    }

    if (store->show_full_tree) {
      /* all entries are visible without scrolling */
      hide_group(help_dlg->pBeginActiveWidgetList,
                 help_dlg->pEndActiveWidgetList);
      hide_scrollbar(help_dlg->pScroll);
    } else {
      int count = help_dlg->pScroll->active;

      pAdvanceLabel = help_dlg->pActiveWidgetList;
      while (pAdvanceLabel && count--) {
        pAdvanceLabel = pAdvanceLabel->prev;
      }
      pAdvanceLabel = pAdvanceLabel->next;
      show_group(pAdvanceLabel, help_dlg->pActiveWidgetList);
      show_scrollbar(help_dlg->pScroll);
    }
  }

  /* --------------------------------------------------------- */
  if (created) {

    surf = theme_get_background(theme, BACKGROUND_HELPDLG);
    if (resize_window(pwindow, surf, NULL, adj_size(640), adj_size(480))) {
      FREESURFACE(surf);
    }

    area = pwindow->area;

    widget_set_position(pwindow,
                        (main_window_width() - pwindow->size.w) / 2,
                        (main_window_height() - pwindow->size.h) / 2);

    /* exit button */
    pCloseButton = pwindow->prev;
    widget_set_position(pCloseButton,
                        area.x + area.w - pCloseButton->size.w - 1,
                        pwindow->size.y + adj_size(2));

    /* list toggle button */
    pListToggleButton = store->dock;
    widget_set_position(pListToggleButton, area.x, area.y);

    /* list entries */
    h = setup_vertical_widgets_position(1, area.x + scrollbar_width,
                                        area.y + pListToggleButton->size.h, 0, 0,
                                        help_dlg->pBeginActiveWidgetList,
                                        help_dlg->pEndActiveWidgetList);
    /* scrollbar */
    if (help_dlg->pScroll) {
      setup_vertical_scrollbar_area(help_dlg->pScroll,
                                    area.x, area.y + pListToggleButton->size.h,
                                    h, FALSE);
    }
  }

  if (store->show_tree) {
    help_dlg->begin_widget_list = create_tech_tree(tech, scrollbar_width, pwindow, store);
    redraw_tech_tree_dlg();
  } else {
    help_dlg->begin_widget_list = create_tech_info(tech, scrollbar_width, pwindow, store);
    redraw_tech_info_dlg();
  }
}
