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
#include "government.h"
#include "research.h"

/* client */
#include "client_main.h"

/* gui-sdl2 */
#include "graphics.h"
#include "gui_id.h"
#include "gui_main.h"
#include "gui_tilespec.h"
#include "mapview.h"
#include "repodlgs.h"
#include "spaceshipdlg.h"
#include "sprite.h"
#include "widget.h"

#include "inteldlg.h"

struct intel_dialog {
  struct player *pplayer;
  struct advanced_dialog *pdialog;
  int pos_x, pos_y;
};

#define SPECLIST_TAG dialog
#define SPECLIST_TYPE struct intel_dialog
#include "speclist.h"

#define dialog_list_iterate(dialoglist, pdialog) \
    TYPED_LIST_ITERATE(struct intel_dialog, dialoglist, pdialog)
#define dialog_list_iterate_end  LIST_ITERATE_END

static struct dialog_list *dialog_list;
static struct intel_dialog *create_intel_dialog(struct player *p);

/**********************************************************************//**
  Allocate intelligence dialog
**************************************************************************/
void intel_dialog_init(void)
{
  dialog_list = dialog_list_new();
}

/**********************************************************************//**
  Free intelligence dialog
**************************************************************************/
void intel_dialog_done(void)
{
  dialog_list_destroy(dialog_list);
}

/**********************************************************************//**
  Get intelligence dialog towards given player
**************************************************************************/
static struct intel_dialog *get_intel_dialog(struct player *pplayer)
{
  dialog_list_iterate(dialog_list, pdialog) {
    if (pdialog->pplayer == pplayer) {
      return pdialog;
    }
  } dialog_list_iterate_end;

  return NULL;
}

/**********************************************************************//**
  User interacted with the intelligence dialog window
**************************************************************************/
static int intel_window_dlg_callback(struct widget *pwindow)
{
  if (PRESSED_EVENT(main_data.event)) {
    struct intel_dialog *pSelectedDialog = get_intel_dialog(pwindow->data.player);

    move_window_group(pSelectedDialog->pdialog->begin_widget_list, pwindow);
  }

  return -1;
}

/**********************************************************************//**
  User interacted with tech widget
**************************************************************************/
static int tech_callback(struct widget *pwidget)
{
  /* get tech help - PORT ME */
  return -1;
}

/**********************************************************************//**
  User interacted with spaceship widget
**************************************************************************/
static int spaceship_callback(struct widget *pwidget)
{
  if (PRESSED_EVENT(main_data.event)) {
    struct player *pplayer = pwidget->data.player;

    popdown_intel_dialog(pplayer);
    popup_spaceship_dialog(pplayer);
  }

  return -1;
}

/**********************************************************************//**
  User interacted with intelligence dialog close button
**************************************************************************/
static int exit_intel_dlg_callback(struct widget *pwidget)
{
  if (PRESSED_EVENT(main_data.event)) {
    popdown_intel_dialog(pwidget->data.player);
    flush_dirty();
  }

  return -1;
}

/**********************************************************************//**
  Close an intelligence dialog towards given player.
**************************************************************************/
void close_intel_dialog(struct player *p)
{
  popdown_intel_dialog(p);
}

/**********************************************************************//**
  Create an intelligence dialog towards given player.
**************************************************************************/
static struct intel_dialog *create_intel_dialog(struct player *pplayer)
{
  struct intel_dialog *pdialog = fc_calloc(1, sizeof(struct intel_dialog));

  pdialog->pplayer = pplayer;

  pdialog->pdialog = fc_calloc(1, sizeof(struct advanced_dialog));

  pdialog->pos_x = 0;
  pdialog->pos_y = 0;

  dialog_list_prepend(dialog_list, pdialog);

  return pdialog;
}

/**********************************************************************//**
  Popup an intelligence dialog for the given player.
**************************************************************************/
void popup_intel_dialog(struct player *p)
{
  struct intel_dialog *pdialog;

  if (!(pdialog = get_intel_dialog(p))) {
    pdialog = create_intel_dialog(p);
  } else {
    /* bring existing dialog to front */
    select_window_group_dialog(pdialog->pdialog->begin_widget_list,
                               pdialog->pdialog->end_widget_list);
  }

  update_intel_dialog(p);
}

/**********************************************************************//**
  Popdown an intelligence dialog for the given player.
**************************************************************************/
void popdown_intel_dialog(struct player *p)
{
  struct intel_dialog *pdialog = get_intel_dialog(p);

  if (pdialog) {
    popdown_window_group_dialog(pdialog->pdialog->begin_widget_list,
                                pdialog->pdialog->end_widget_list);

    dialog_list_remove(dialog_list, pdialog);

    FC_FREE(pdialog->pdialog->pScroll);
    FC_FREE(pdialog->pdialog);
    FC_FREE(pdialog);
  }
}

/**********************************************************************//**
  Popdown all intelligence dialogs
**************************************************************************/
void popdown_intel_dialogs(void)
{
  dialog_list_iterate(dialog_list, pdialog) {
    popdown_intel_dialog(pdialog->pplayer);
  } dialog_list_iterate_end;
}

/**********************************************************************//**
  Update the intelligence dialog for the given player.  This is called by
  the core client code when that player's information changes.
**************************************************************************/
void update_intel_dialog(struct player *p)
{
  const struct research *mresearch, *presearch;
  struct intel_dialog *pdialog = get_intel_dialog(p);
  struct widget *pwindow = NULL, *pBuf = NULL, *pLast;
  SDL_Surface *logo = NULL, *tmp_surf = NULL;
  SDL_Surface *pText1, *pInfo, *pText2 = NULL;
  utf8_str *pstr;
  SDL_Rect dst;
  char cBuf[256], plr_buf[4 * MAX_LEN_NAME];
  int n = 0, count = 0, col;
  struct city *pcapital;
  SDL_Rect area;
  struct research *research;

  if (pdialog) {
    /* save window position and delete old content */
    if (pdialog->pdialog->end_widget_list) {
      pdialog->pos_x = pdialog->pdialog->end_widget_list->size.x;
      pdialog->pos_y = pdialog->pdialog->end_widget_list->size.y;

      popdown_window_group_dialog(pdialog->pdialog->begin_widget_list,
                                  pdialog->pdialog->end_widget_list);
    }

    pstr = create_utf8_from_char(_("Foreign Intelligence Report") , adj_font(12));
    pstr->style |= TTF_STYLE_BOLD;

    pwindow = create_window_skeleton(NULL, pstr, 0);

    pwindow->action = intel_window_dlg_callback;
    set_wstate(pwindow , FC_WS_NORMAL);
    pwindow->data.player = p;

    add_to_gui_list(ID_WINDOW, pwindow);
    pdialog->pdialog->end_widget_list = pwindow;

    area = pwindow->area;

    /* ---------- */
    /* exit button */
    pBuf = create_themeicon(current_theme->Small_CANCEL_Icon, pwindow->dst,
                            WF_WIDGET_HAS_INFO_LABEL
                            | WF_RESTORE_BACKGROUND);
    pBuf->info_label = create_utf8_from_char(_("Close Dialog (Esc)"),
                                             adj_font(12));
    area.w = MAX(area.w, pBuf->size.w + adj_size(10));
    pBuf->action = exit_intel_dlg_callback;
    set_wstate(pBuf, FC_WS_NORMAL);
    pBuf->data.player = p;
    pBuf->key = SDLK_ESCAPE;

    add_to_gui_list(ID_BUTTON, pBuf);
    /* ---------- */

    logo = get_nation_flag_surface(nation_of_player(p));

    {
      double zoom = DEFAULT_ZOOM * 60.0 / logo->h;

      pText1 = zoomSurface(logo, zoom, zoom, 1);
    }

    logo = pText1;

    pBuf = create_icon2(logo, pwindow->dst,
                        WF_RESTORE_BACKGROUND | WF_WIDGET_HAS_INFO_LABEL
                        | WF_FREE_THEME);
    pBuf->action = spaceship_callback;
    set_wstate(pBuf, FC_WS_NORMAL);
    pBuf->data.player = p;
    fc_snprintf(cBuf, sizeof(cBuf),
                _("Intelligence Information about the %s Spaceship"),
                nation_adjective_for_player(p));
    pBuf->info_label = create_utf8_from_char(cBuf, adj_font(12));

    add_to_gui_list(ID_ICON, pBuf);

    /* ---------- */
    fc_snprintf(cBuf, sizeof(cBuf),
                _("Intelligence Information for the %s Empire"),
                nation_adjective_for_player(p));

    pstr = create_utf8_from_char(cBuf, adj_font(14));
    pstr->style |= TTF_STYLE_BOLD;
    pstr->bgcol = (SDL_Color) {0, 0, 0, 0};

    pText1 = create_text_surf_from_utf8(pstr);
    area.w = MAX(area.w, pText1->w + adj_size(20));
    area.h += pText1->h + adj_size(20);

    /* ---------- */

    pcapital = player_primary_capital(p);
    research = research_get(p);
    change_ptsize_utf8(pstr, adj_font(10));
    pstr->style &= ~TTF_STYLE_BOLD;

    /* FIXME: these should use common gui code, and avoid duplication! */
    switch (research->researching) {
    case A_UNKNOWN:
    case A_UNSET:
      fc_snprintf(cBuf, sizeof(cBuf),
                  _("Ruler: %s  Government: %s\n"
                    "Capital: %s  Gold: %d\n"
                    "Tax: %d%% Science: %d%% Luxury: %d%%\n"
                    "Researching: unknown\n"
                    "Culture: %d"),
                  ruler_title_for_player(p, plr_buf, sizeof(plr_buf)),
                  government_name_for_player(p),
                  /* TRANS: "unknown" location */
                  NULL != pcapital ? city_name_get(pcapital) : _("(unknown)"),
                  p->economic.gold, p->economic.tax,
                  p->economic.science, p->economic.luxury, p->client.culture);
      break;
    default:
      fc_snprintf(cBuf, sizeof(cBuf),
                  _("Ruler: %s  Government: %s\n"
                    "Capital: %s  Gold: %d\n"
                    "Tax: %d%% Science: %d%% Luxury: %d%%\n"
                    "Researching: %s(%d/%d)\n"
                    "Culture: %d"),
                  ruler_title_for_player(p, plr_buf, sizeof(plr_buf)),
                  government_name_for_player(p),
                  /* TRANS: "unknown" location */
                  NULL != pcapital ? city_name_get(pcapital) : _("(unknown)"),
                  p->economic.gold, p->economic.tax, p->economic.science,
                  p->economic.luxury,
                  research_advance_name_translation(research,
                                                    research->researching),
                  research->bulbs_researched,
                  research->client.researching_cost, p->client.culture);
      break;
    };

    copy_chars_to_utf8_str(pstr, cBuf);
    pInfo = create_text_surf_from_utf8(pstr);
    area.w = MAX(area.w, logo->w + adj_size(10) + pInfo->w + adj_size(20));
    area.h += MAX(logo->h + adj_size(20), pInfo->h + adj_size(20));

    /* ---------- */
    tmp_surf = get_tech_icon(A_FIRST);
    col = area.w / (tmp_surf->w + adj_size(4));
    FREESURFACE(tmp_surf);
    n = 0;
    pLast = pBuf;
    mresearch = research_get(client_player());
    presearch = research_get(p);
    advance_index_iterate(A_FIRST, i) {
      if (TECH_KNOWN == research_invention_state(presearch, i)
          && research_invention_reachable(mresearch, i)
          && TECH_KNOWN != research_invention_state(mresearch, i)) {

        pBuf = create_icon2(get_tech_icon(i), pwindow->dst,
                            WF_RESTORE_BACKGROUND | WF_WIDGET_HAS_INFO_LABEL
                            | WF_FREE_THEME);
        pBuf->action = tech_callback;
        set_wstate(pBuf, FC_WS_NORMAL);

        pBuf->info_label =
            create_utf8_from_char(advance_name_translation
                                  (advance_by_number(i)), adj_font(12));

        add_to_gui_list(ID_ICON, pBuf);

        if (n > ((2 * col) - 1)) {
          set_wflag(pBuf, WF_HIDDEN);
        }

        n++;
      }
    } advance_index_iterate_end;

    pdialog->pdialog->begin_widget_list = pBuf;

    if (n > 0) {
      pdialog->pdialog->pEndActiveWidgetList = pLast->prev;
      pdialog->pdialog->pBeginActiveWidgetList = pdialog->pdialog->begin_widget_list;
      if (n > 2 * col) {
        pdialog->pdialog->pActiveWidgetList = pdialog->pdialog->pEndActiveWidgetList;
        count = create_vertical_scrollbar(pdialog->pdialog, col, 2, TRUE, TRUE);
        area.h += (2 * pBuf->size.h + adj_size(10));
      } else {
        count = 0;
        if (n > col) {
          area.h += pBuf->size.h;
        }
        area.h += (adj_size(10) + pBuf->size.h);
      }

      area.w = MAX(area.w, col * pBuf->size.w + count);

      fc_snprintf(cBuf, sizeof(cBuf), _("Their techs that we don't have :"));
      copy_chars_to_utf8_str(pstr, cBuf);
      pstr->style |= TTF_STYLE_BOLD;
      pText2 = create_text_surf_from_utf8(pstr);
    }

    FREEUTF8STR(pstr);

    resize_window(pwindow, NULL, NULL,
                  (pwindow->size.w - pwindow->area.w) + area.w,
                  (pwindow->size.h - pwindow->area.h) + area.h);

    area = pwindow->area;

    /* ------------------------ */
    widget_set_position(pwindow,
      (pdialog->pos_x) ? (pdialog->pos_x) : ((main_window_width() - pwindow->size.w) / 2),
      (pdialog->pos_y) ? (pdialog->pos_y) : ((main_window_height() - pwindow->size.h) / 2));

    /* exit button */
    pBuf = pwindow->prev;
    pBuf->size.x = area.x + area.w - pBuf->size.w - 1;
    pBuf->size.y = pwindow->size.y + adj_size(2);

    dst.x = area.x + (area.w - pText1->w) / 2;
    dst.y = area.y + adj_size(8);

    alphablit(pText1, NULL, pwindow->theme, &dst, 255);
    dst.y += pText1->h + adj_size(10);
    FREESURFACE(pText1);

    /* spaceship button */
    pBuf = pBuf->prev;
    dst.x = area.x + (area.w - (pBuf->size.w + adj_size(10) + pInfo->w)) / 2;
    pBuf->size.x = dst.x;
    pBuf->size.y = dst.y;

    dst.x += pBuf->size.w + adj_size(10);
    alphablit(pInfo, NULL, pwindow->theme, &dst, 255);
    dst.y += pInfo->h + adj_size(10);
    FREESURFACE(pInfo);

    /* --------------------- */

    if (n) {
      dst.x = area.x + adj_size(5);
      alphablit(pText2, NULL, pwindow->theme, &dst, 255);
      dst.y += pText2->h + adj_size(2);
      FREESURFACE(pText2);

      setup_vertical_widgets_position(col, area.x, dst.y, 0, 0,
                                      pdialog->pdialog->pBeginActiveWidgetList,
                                      pdialog->pdialog->pEndActiveWidgetList);

      if (pdialog->pdialog->pScroll) {
        setup_vertical_scrollbar_area(pdialog->pdialog->pScroll,
                                      area.x + area.w, dst.y,
                                      area.h - (dst.y + 1), TRUE);
      }
    }

    redraw_group(pdialog->pdialog->begin_widget_list, pdialog->pdialog->end_widget_list, 0);
    widget_mark_dirty(pwindow);

    flush_dirty();
  }
}
