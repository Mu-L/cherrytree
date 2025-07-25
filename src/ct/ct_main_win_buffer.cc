/*
 * ct_main_win_buffer.cc
 *
 * Copyright 2009-2024
 * Giuseppe Penone <giuspen@gmail.com>
 * Evgenii Gurianov <https://github.com/txe>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include "ct_main_win.h"
#include "ct_storage_xml.h"
#include "ct_export2txt.h"

void CtMainWin::apply_syntax_highlighting(Glib::RefPtr<Gtk::TextBuffer> pTextBuffer,
                                          const std::string& syntax,
                                          const bool forceReApply)
{
    if (not forceReApply and pTextBuffer->get_data(CtConst::STYLE_APPLIED_ID)) {
        return;
    }
    GtkSourceStyleSchemeManager* pGtkSourceStyleSchemeManager = gtk_source_style_scheme_manager_get_default();
    auto pGtkSourceBuffer = GTK_SOURCE_BUFFER(pTextBuffer->gobj());
    auto f_applyScheme = [pGtkSourceStyleSchemeManager, pGtkSourceBuffer](const char* scheme){
        GtkSourceStyleScheme* pGtkSourceStyleScheme = gtk_source_style_scheme_manager_get_scheme(pGtkSourceStyleSchemeManager, scheme);
        if (pGtkSourceStyleScheme) {
            gtk_source_buffer_set_style_scheme(pGtkSourceBuffer, pGtkSourceStyleScheme);
        }
        else {
            spdlog::error("!! {} pGtkSourceStyleScheme '{}'", __FUNCTION__, scheme);
        }
    };
    if (CtConst::TABLE_CELL_TEXT_ID == syntax) {
        f_applyScheme(_pCtConfig->taStyleScheme.c_str());
        gtk_source_buffer_set_highlight_matching_brackets(pGtkSourceBuffer, _pCtConfig->rtHighlMatchBra);
        gtk_source_buffer_set_highlight_syntax(pGtkSourceBuffer, false);
    }
    else if (CtConst::RICH_TEXT_ID == syntax) {
        f_applyScheme(_pCtConfig->rtStyleScheme.c_str());
        gtk_source_buffer_set_highlight_matching_brackets(pGtkSourceBuffer, _pCtConfig->rtHighlMatchBra);
        gtk_source_buffer_set_highlight_syntax(pGtkSourceBuffer, false);
    }
    else {
        if (CtConst::PLAIN_TEXT_ID == syntax) {
            f_applyScheme(_pCtConfig->ptStyleScheme.c_str());
            gtk_source_buffer_set_highlight_syntax(pGtkSourceBuffer, false);
        }
        else {
            f_applyScheme(_pCtConfig->coStyleScheme.c_str());
            GtkSourceLanguage* pGtkSourceLanguage = gtk_source_language_manager_get_language(get_language_manager(), syntax.c_str());
            if (pGtkSourceLanguage) {
                gtk_source_buffer_set_language(pGtkSourceBuffer, pGtkSourceLanguage);
                gtk_source_buffer_set_highlight_syntax(pGtkSourceBuffer, true);
            }
            else {
                spdlog::error("!! {} pGtkSourceLanguage '{}'", __FUNCTION__, syntax);
            }
        }
        gtk_source_buffer_set_highlight_matching_brackets(pGtkSourceBuffer, _pCtConfig->ptHighlMatchBra);
    }
    pTextBuffer->set_data(CtConst::STYLE_APPLIED_ID, (void*)1);
}

void CtMainWin::resetup_for_syntax(const char target/*'r':RichText, 'p':PlainTextNCode*/)
{
    CtTreeIter treeIter = curr_tree_iter();
    if ('r' == target) {
        // we have to reapply only if the selected node is rich text
        if (treeIter and treeIter.get_node_syntax_highlighting() == CtConst::RICH_TEXT_ID) {
            _ctTextview.setup_for_syntax(treeIter.get_node_syntax_highlighting());
        }
    }
    else if ('p' == target) {
        // we have to reapply only if the selected node is rich text
        if (treeIter and treeIter.get_node_syntax_highlighting() != CtConst::RICH_TEXT_ID) {
            _ctTextview.setup_for_syntax(treeIter.get_node_syntax_highlighting());
        }
        // we need also to reapply to all codeboxes that were already loaded
        get_tree_store().get_store()->foreach([&](const Gtk::TreePath&/*treePath*/, const Gtk::TreeModel::iterator& treeIter)->bool
        {
            CtTreeIter node = get_tree_store().to_ct_tree_iter(treeIter);
            if (node.get_node_is_rich_text() and node.get_node_buffer_already_loaded()) {
                // let's look for codeboxes
                std::list<CtAnchoredWidget*> anchoredWidgets = node.get_anchored_widgets_fast();
                for (auto pAnchoredWidget : anchoredWidgets) {
                    if (CtAnchWidgType::CodeBox == pAnchoredWidget->get_type()) {
                        CtCodebox* pCodebox = dynamic_cast<CtCodebox*>(pAnchoredWidget);
                        if (pCodebox) {
                            pCodebox->get_text_view().setup_for_syntax(pCodebox->get_syntax_highlighting());
                        }
                    }
                }
            }
            return false; /* false for continue */
        });
    }
    else {
        spdlog::debug("bad reapply target {}", target);
    }
}

void CtMainWin::codeboxes_reload_toolbar()
{
    get_tree_store().get_store()->foreach([&](const Gtk::TreePath&/*treePath*/, const Gtk::TreeModel::iterator& treeIter)->bool
    {
        CtTreeIter node = get_tree_store().to_ct_tree_iter(treeIter);
        if (node.get_node_is_rich_text() and node.get_node_buffer_already_loaded()) {
            // let's look for codeboxes
            std::list<CtAnchoredWidget*> anchoredWidgets = node.get_anchored_widgets_fast();
            for (auto pAnchoredWidget : anchoredWidgets) {
                if (CtAnchWidgType::CodeBox == pAnchoredWidget->get_type()) {
                    CtCodebox* pCodebox = dynamic_cast<CtCodebox*>(pAnchoredWidget);
                    if (pCodebox) {
                        pCodebox->update_toolbar_buttons();
                    }
                }
            }
        }
        return false; /* false for continue */
    });
}

void CtMainWin::reapply_syntax_highlighting(const char target/*'r':RichText, 'p':PlainTextNCode, 't':Table*/)
{
    std::string error;
    get_tree_store().get_store()->foreach([&](const Gtk::TreePath& /*treePath*/, const Gtk::TreeModel::iterator& treeIter)->bool
    {
        CtTreeIter node = get_tree_store().to_ct_tree_iter(treeIter);
        switch (target) {
            case 'r': {
                if (node.get_node_is_rich_text()) {
                    Glib::RefPtr<Gtk::TextBuffer> pTextBuffer = node.get_node_text_buffer();
                    if (not pTextBuffer) {
                        error = str::format(_("Failed to retrieve the content of the node '%s'"), node.get_node_name().raw());
                        return true; /* true for stop */
                    }
                    apply_syntax_highlighting(pTextBuffer, node.get_node_syntax_highlighting(), true/*forceReApply*/);
                }
            } break;
            case 'p': {
                if (node.get_node_is_rich_text() and node.get_node_buffer_already_loaded()) {
                    // let's look for codeboxes
                    std::list<CtAnchoredWidget*> anchoredWidgets = node.get_anchored_widgets_fast();
                    for (auto pAnchoredWidget : anchoredWidgets) {
                        if (CtAnchWidgType::CodeBox == pAnchoredWidget->get_type()) {
                            pAnchoredWidget->apply_syntax_highlighting(true/*forceReApply*/);
                        }
                    }
                }
                else {
                    Glib::RefPtr<Gtk::TextBuffer> pTextBuffer = node.get_node_text_buffer();
                    if (not pTextBuffer) {
                        error = str::format(_("Failed to retrieve the content of the node '%s'"), node.get_node_name().raw());
                        return true; /* true for stop */
                    }
                    apply_syntax_highlighting(pTextBuffer, node.get_node_syntax_highlighting(), true/*forceReApply*/);
                }
            } break;
            case 't': {
                if (node.get_node_is_rich_text() and node.get_node_buffer_already_loaded()) {
                    // let's look for tables
                    std::list<CtAnchoredWidget*> anchoredWidgets = node.get_anchored_widgets_fast();
                    for (auto pAnchoredWidget : anchoredWidgets) {
                        if (CtAnchWidgType::TableHeavy == pAnchoredWidget->get_type()) {
                            pAnchoredWidget->apply_syntax_highlighting(true/*forceReApply*/);
                        }
                    }
                }
            } break;
            default: spdlog::debug("bad reapply target {}", target);
        }
        return false; /* false for continue */
    });
    if (not error.empty()) CtDialogs::error_dialog(error, *this);
}

Glib::RefPtr<Gtk::TextBuffer> CtMainWin::get_new_text_buffer(const Glib::ustring& textContent)
{
    GtkSourceBuffer* pGtkSourceBuffer = gtk_source_buffer_new(_rGtkTextTagTable->gobj());
    Glib::RefPtr<Gtk::TextBuffer> rRetTextBuffer = Glib::wrap(GTK_TEXT_BUFFER(pGtkSourceBuffer));
    gtk_source_buffer_set_max_undo_levels(pGtkSourceBuffer, _pCtConfig->limitUndoableSteps);

    if (not textContent.empty()) {
        gtk_source_buffer_begin_not_undoable_action(pGtkSourceBuffer);
        rRetTextBuffer->set_text(textContent);
        gtk_source_buffer_end_not_undoable_action(pGtkSourceBuffer);
        rRetTextBuffer->set_modified(false);
    }
    return rRetTextBuffer;
}

void CtMainWin::apply_scalable_properties(Glib::RefPtr<Gtk::TextTag> rTextTag, CtScalableTag* pCtScalableTag)
{
    rTextTag->property_scale() = pCtScalableTag->scale;
    if (not pCtScalableTag->foreground.empty()) {
        rTextTag->property_foreground() = pCtScalableTag->foreground;
    }
    if (not pCtScalableTag->background.empty()) {
        rTextTag->property_background() = pCtScalableTag->background;
    }
    if (pCtScalableTag->bold) {
        rTextTag->property_weight() = Pango::Weight::WEIGHT_HEAVY;
    }
    if (pCtScalableTag->italic) {
        rTextTag->property_style() = Pango::Style::STYLE_ITALIC;
    }
    if (pCtScalableTag->underline) {
        rTextTag->property_underline() = Pango::Underline::UNDERLINE_SINGLE;
    }
}

std::string CtMainWin::get_text_tag_name_exist_or_create(const std::string& propertyName,
                                                         const std::string& propertyValue)
{
    const std::string tagName{propertyName + "_" + propertyValue};
    Glib::RefPtr<Gtk::TextTag> rTextTag = _rGtkTextTagTable->lookup(tagName);
    if (not rTextTag) {
        bool identified{true};
        rTextTag = Gtk::TextTag::create(tagName);
        if (CtConst::TAG_INDENT == propertyName) {
            rTextTag->property_left_margin() = CtConst::INDENT_MARGIN * std::stoi(propertyValue) + _pCtConfig->textMarginLeft;
            rTextTag->property_indent() = 0;
        }
        else if (CtConst::TAG_WEIGHT == propertyName and CtConst::TAG_PROP_VAL_HEAVY == propertyValue) {
            rTextTag->property_weight() = Pango::Weight::WEIGHT_HEAVY;
        }
        else if (CtConst::TAG_FOREGROUND == propertyName) {
            rTextTag->property_foreground() = propertyValue;
        }
        else if (CtConst::TAG_BACKGROUND == propertyName) {
            rTextTag->property_background() = propertyValue;
        }
        else if (CtConst::TAG_SCALE == propertyName) {
            if (CtConst::TAG_PROP_VAL_SMALL == propertyValue) {
                apply_scalable_properties(rTextTag, &_pCtConfig->scalableSmall);
            }
            else if (CtConst::TAG_PROP_VAL_H1 == propertyValue) {
                apply_scalable_properties(rTextTag, &_pCtConfig->scalableH1);
            }
            else if (CtConst::TAG_PROP_VAL_H2 == propertyValue) {
                apply_scalable_properties(rTextTag, &_pCtConfig->scalableH2);
            }
            else if (CtConst::TAG_PROP_VAL_H3 == propertyValue) {
                apply_scalable_properties(rTextTag, &_pCtConfig->scalableH3);
            }
            else if (CtConst::TAG_PROP_VAL_H4 == propertyValue) {
                apply_scalable_properties(rTextTag, &_pCtConfig->scalableH4);
            }
            else if (CtConst::TAG_PROP_VAL_H5 == propertyValue) {
                apply_scalable_properties(rTextTag, &_pCtConfig->scalableH5);
            }
            else if (CtConst::TAG_PROP_VAL_H6 == propertyValue) {
                apply_scalable_properties(rTextTag, &_pCtConfig->scalableH6);
            }
            else if (CtConst::TAG_PROP_VAL_SUB == propertyValue or CtConst::TAG_PROP_VAL_SUP == propertyValue) {
                rTextTag->property_scale() = PANGO_SCALE_X_SMALL;
                int propRise = Pango::FontDescription(_pCtConfig->rtFont).get_size();
                if (CtConst::TAG_PROP_VAL_SUB == propertyValue) {
                    propRise /= -4;
                }
                else {
                    propRise /= 2;
                }
                rTextTag->property_rise() = propRise;
            }
            else {
                identified = false;
            }
        }
        else if (CtConst::TAG_INVISIBLE == propertyName) {
            rTextTag->property_invisible() = true;
        }
        else if (CtConst::TAG_STYLE == propertyName and CtConst::TAG_PROP_VAL_ITALIC == propertyValue) {
            rTextTag->property_style() = Pango::Style::STYLE_ITALIC;
        }
        else if (CtConst::TAG_UNDERLINE == propertyName and CtConst::TAG_PROP_VAL_SINGLE == propertyValue) {
            rTextTag->property_underline() = Pango::Underline::UNDERLINE_SINGLE;
        }
        else if (CtConst::TAG_JUSTIFICATION == propertyName) {
            if (CtConst::TAG_PROP_VAL_LEFT == propertyValue) {
                rTextTag->property_justification() = Gtk::Justification::JUSTIFY_LEFT; 
            }
            else if (CtConst::TAG_PROP_VAL_RIGHT == propertyValue) {
                rTextTag->property_justification() = Gtk::Justification::JUSTIFY_RIGHT;
            }
            else if (CtConst::TAG_PROP_VAL_CENTER == propertyValue) {
                rTextTag->property_justification() = Gtk::Justification::JUSTIFY_CENTER;
            }
            else if (CtConst::TAG_PROP_VAL_FILL == propertyValue) {
                rTextTag->property_justification() = Gtk::Justification::JUSTIFY_FILL;
            }
            else {
                identified = false;
            }
        }
        else if (CtConst::TAG_FAMILY == propertyName and CtConst::TAG_PROP_VAL_MONOSPACE == propertyValue) {
            rTextTag->property_family() = CtConst::TAG_PROP_VAL_MONOSPACE;
            if (not _pCtConfig->monospaceFg.empty()) {
                rTextTag->property_foreground() = _pCtConfig->monospaceFg;
            }
            if (not _pCtConfig->monospaceBg.empty()) {
                rTextTag->property_background() = _pCtConfig->monospaceBg;
            }
            if (_pCtConfig->msDedicatedFont and not _pCtConfig->monospaceFont.empty()) {
                rTextTag->property_font() = _pCtConfig->monospaceFont;
            }
            else {
                rTextTag->property_family() = CtConst::TAG_PROP_VAL_MONOSPACE;
            }
        }
        else if (CtConst::TAG_STRIKETHROUGH == propertyName and CtConst::TAG_PROP_VAL_TRUE == propertyValue) {
            rTextTag->property_strikethrough() = true;
        }
        else if (CtConst::TAG_LINK == propertyName and propertyValue.size() > 4) {
            if (_pCtConfig->linksUnderline) {
                rTextTag->property_underline() = Pango::Underline::UNDERLINE_SINGLE;
            }
            Glib::ustring linkType = propertyValue.substr(0, 4);
            if (CtConst::LINK_TYPE_WEBS == linkType) {
                rTextTag->property_foreground() = _pCtConfig->colLinkWebs;
            }
            else if (CtConst::LINK_TYPE_NODE == linkType) {
                rTextTag->property_foreground() = _pCtConfig->colLinkNode;
            }
            else if (CtConst::LINK_TYPE_FILE == linkType) {
                rTextTag->property_foreground() = _pCtConfig->colLinkFile;
            }
            else if (CtConst::LINK_TYPE_FOLD == linkType) {
                rTextTag->property_foreground() = _pCtConfig->colLinkFold;
            }
            else {
                identified = false;
            }
        }
        else {
            identified = false;
        }
        if (not identified) {
           // spdlog::error("!! unsupported propertyName={} propertyValue={}", propertyName, propertyValue);
        }
        _rGtkTextTagTable->add(rTextTag);
    }
    return tagName;
}

// Get the tooltip for the underlying link
Glib::ustring CtMainWin::sourceview_hovering_link_get_tooltip(const Glib::ustring& link)
{
    CtLinkEntry link_entry = CtMiscUtil::get_link_entry_from_property(link);
    Glib::ustring tooltip;
    if (CtLinkType::None == link_entry.type) { // case when link has wrong format
        tooltip = str::replace(link, "amp;", "");
    }
    else if (CtLinkType::Webs == link_entry.type) {
        tooltip = str::replace(link_entry.webs, "amp;", "");
    }
    else if (CtLinkType::File == link_entry.type) {
        tooltip = link_entry.file;
    }
    else if (CtLinkType::Fold == link_entry.type) {
        tooltip = link_entry.fold;
    }
    else if (CtLinkType::Node == link_entry.type) {
        tooltip = _uCtTreestore->get_node_name_from_node_id(link_entry.node_id);
        if (!link_entry.anch.empty())
            tooltip += "#" + link_entry.anch;
    }
    return tooltip;
}

// Try to Select a Word Forward/Backward the Cursor
bool CtMainWin::apply_tag_try_automatic_bounds(Glib::RefPtr<Gtk::TextBuffer> text_buffer,
                                               Gtk::TextIter iter_start)
{
    Gtk::TextIter iter_end = iter_start;
    gunichar curr_char = iter_end.get_char();
    // 1) select alphanumeric + special
    bool is_alphanum = g_unichar_isalnum(curr_char);
    if (not is_alphanum and _pCtConfig->selwordChars.item().find(curr_char) == Glib::ustring::npos) {
        iter_start.backward_char();
        iter_end.backward_char();
        curr_char = iter_end.get_char();
        is_alphanum = g_unichar_isalnum(curr_char);
        if (not is_alphanum and _pCtConfig->selwordChars.item().find(curr_char) == Glib::ustring::npos)
            return false;
    }
    while (is_alphanum or _pCtConfig->selwordChars.item().find(curr_char) != Glib::ustring::npos) {
        if (not iter_end.forward_char()) break; // end of buffer
        curr_char = iter_end.get_char();
        is_alphanum = g_unichar_isalnum(curr_char);
    }
    iter_start.backward_char();
    curr_char = iter_start.get_char();
    is_alphanum = g_unichar_isalnum(curr_char);
    while (is_alphanum or _pCtConfig->selwordChars.item().find(curr_char) != Glib::ustring::npos) {
        if (not iter_start.backward_char()) break; // start of buffer
        curr_char = iter_start.get_char();
        is_alphanum = g_unichar_isalnum(curr_char);
    }
    if (not is_alphanum and _pCtConfig->selwordChars.item().find(curr_char) == Glib::ustring::npos)
        iter_start.forward_char();
    // 2) remove non alphanumeric from borders
    iter_end.backward_char();
    curr_char = iter_end.get_char();
    while (_pCtConfig->selwordChars.item().find(curr_char) != Glib::ustring::npos) {
        if (not iter_end.backward_char()) break; // start of buffer
        curr_char = iter_end.get_char();
    }
    iter_end.forward_char();
    curr_char = iter_start.get_char();
    while (_pCtConfig->selwordChars.item().find(curr_char) != Glib::ustring::npos) {
        if (not iter_start.forward_char()) break; // end of buffer
        curr_char = iter_start.get_char();
    }
    if (iter_end.compare(iter_start) > 0) {
        text_buffer->move_mark(text_buffer->get_insert(), iter_start);
        text_buffer->move_mark(text_buffer->get_selection_bound(), iter_end);
        return true;
    }
    return false;
}

// Try to select the full paragraph
void CtMainWin::apply_tag_try_automatic_bounds_paragraph(Glib::RefPtr<Gtk::TextBuffer> text_buffer,
                                                         Gtk::TextIter iter_start)
{
    Gtk::TextIter iter_end = iter_start;
    iter_end.forward_to_line_end();

    iter_end.forward_char();
    auto next_char = iter_end.get_char();
    while (next_char != '\n' && next_char != ' ') {
        // forward to the end of the line, if the next char
        // is not a new line or space then repeat
        iter_end.forward_to_line_end();
        if (!iter_end.forward_char()) break;
        next_char = iter_end.get_char();
    }

    // reverse to beginning of line to check for space indicating line
    // selected is the first line of a paragraph
    iter_start.backward_chars(iter_start.get_visible_line_offset());
    // reverse until either a new line or a space is found
    while (iter_start.get_char() != '\n' && iter_start.get_char() != ' ') {
        if (!iter_start.backward_line()) break;
    }

    if (iter_start.get_char() == '\n') {
        iter_start.forward_chars(1);
    }

    text_buffer->move_mark(text_buffer->get_insert(), iter_start);
    text_buffer->move_mark(text_buffer->get_selection_bound(), iter_end);
}

void CtMainWin::re_load_current_buffer(const bool new_machine_state)
{
    CtTreeIter currTreeIter = curr_tree_iter();
    if (new_machine_state) {
        _ctStateMachine.update_state(currTreeIter);
    }
    std::shared_ptr<CtNodeState> currState = _ctStateMachine.requested_state_current(currTreeIter.get_node_id_data_holder());
    load_buffer_from_state(currState, currTreeIter);
}

// Load Text Buffer from State Machine
void CtMainWin::load_buffer_from_state(std::shared_ptr<CtNodeState> state, CtTreeIter tree_iter)
{
    bool user_active_restore = user_active();
    user_active() = false;

    Glib::RefPtr<Gtk::TextBuffer> pTextBuffer = tree_iter.get_node_text_buffer();
    auto pGtkSourceBuffer = GTK_SOURCE_BUFFER(pTextBuffer->gobj());

    gtk_source_buffer_begin_not_undoable_action(pGtkSourceBuffer);

    // erase is slow on empty buffer
    if (pTextBuffer->begin() != pTextBuffer->end()) {
        pTextBuffer->erase(pTextBuffer->begin(), pTextBuffer->end());
    }
    tree_iter.remove_all_embedded_widgets();
    std::list<CtAnchoredWidget*> widgets;
    for (xmlpp::Node* text_node : state->buffer_xml.get_root_node()->get_children()) {
        CtStorageXmlHelper{this}.get_text_buffer_one_slot_from_xml(pTextBuffer, text_node, widgets, nullptr, -1, "");
    }

    // xml storage doesn't have widgets, so load them separately
    for (auto widgetState : state->widgetStates) {
        widgets.push_back(widgetState->to_widget(this));
    }
    for (auto widget : widgets) {
        widget->insertInTextBuffer(pTextBuffer);
    }
    get_tree_store().addAnchoredWidgets(tree_iter, widgets, &_ctTextview.mm());

    gtk_source_buffer_end_not_undoable_action(pGtkSourceBuffer);
    pTextBuffer->set_modified(false);

    _uCtTreestore->text_view_apply_textbuffer(tree_iter, &_ctTextview);
    _ctTextview.mm().grab_focus();

    _ctTextview.set_spell_check(curr_tree_iter().get_node_is_text());

    pTextBuffer->place_cursor(pTextBuffer->get_iter_at_offset(state->cursor_pos));
    (void)_try_move_focus_to_anchored_widget_if_on_it();

    while (gtk_events_pending()) gtk_main_iteration();
    _scrolledwindowText.get_vadjustment()->set_value(state->v_adj_val);

    user_active() = user_active_restore;

    update_window_save_needed(CtSaveNeededUpdType::nbuf, false, &tree_iter);
}

// Switch TextBuffer -> SourceBuffer or SourceBuffer -> TextBuffer
void CtMainWin::switch_buffer_text_source(Glib::RefPtr<Gtk::TextBuffer> pTextBuffer,
                                          CtTreeIter tree_iter,
                                          const std::string& new_syntax,
                                          const std::string& old_syntax)
{
    if (new_syntax == old_syntax) {
        return;
    }
    bool user_active_restore = user_active();
    user_active() = false;

    Glib::ustring node_text;
    if (old_syntax == CtConst::RICH_TEXT_ID) {
        node_text = CtExport2Txt{this}.node_export_to_txt(tree_iter, "", {0}, -1, -1);
    }
    else {
        node_text = pTextBuffer->get_text();
    }

    auto new_buffer = get_new_text_buffer(node_text);
    tree_iter.set_node_text_buffer(new_buffer, new_syntax);
    _uCtTreestore->text_view_apply_textbuffer(tree_iter, &_ctTextview);

    user_active() = user_active_restore;
}

void CtMainWin::text_view_apply_cursor_position(CtTreeIter& treeIter, const int cursor_pos, const int v_adj_val)
{
    Glib::RefPtr<Gtk::TextBuffer> pTextBuffer = treeIter.get_node_text_buffer();
    Gtk::TextIter textIter = pTextBuffer->get_iter_at_offset(cursor_pos);
    // if (static_cast<bool>(textIter)) <- don't check because iter at the end returns false

    pTextBuffer->place_cursor(textIter);

    while (gtk_events_pending()) gtk_main_iteration();
    _scrolledwindowText.get_vadjustment()->set_value(v_adj_val);
}

bool CtMainWin::_try_move_focus_to_anchored_widget_if_on_it()
{
    auto iter_insert = _ctTextview.get_buffer()->get_insert()->get_iter();
    auto widgets = curr_tree_iter().get_anchored_widgets(iter_insert.get_offset(), iter_insert.get_offset());
    if (not widgets.empty()) {
        if (CtCodebox* pCodebox = dynamic_cast<CtCodebox*>(widgets.front())) {
            pCodebox->get_text_view().mm().grab_focus();
            return true;
        }
        if (auto pTable = dynamic_cast<CtTableCommon*>(widgets.front())) {
            pTable->grab_focus();
            return true;
        }
    }
    return false;
}
