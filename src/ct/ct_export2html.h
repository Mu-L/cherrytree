/*
 * ct_export2html.h
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

#pragma once

#include "ct_table.h"
#include "ct_codebox.h"
#include "ct_image.h"
#include "ct_treestore.h"
#include "ct_dialogs.h" // CtExportOptions
#include "ct_misc_utils.h"

class CtExport2Html
{
private:
    const Glib::ustring HTML_HEADER = R"HTML(<!doctype html>
<html>
<head>
  <meta http-equiv="content-type" content="text/html; charset=utf-8">
  <title>%s</title>
  <meta name="generator" content="CherryTree">
  <link rel="stylesheet" href="res/styles4.css" type="text/css" />
</head>
<body>
)HTML"; // after <body> should not be any whitespaces
    const Glib::ustring HTML_FOOTER = R"HTML(
</body>
</html>
)HTML";

public:
    CtExport2Html(CtMainWin* pCtMainWin);

    static Glib::ustring html_process_slot(const CtConfig* const pCtConfig,
                                           CtMainWin* const pCtMainWin, // the unit tests may pass nullptr here!
                                           int start_offset,
                                           int end_offset,
                                           Glib::RefPtr<Gtk::TextBuffer> curr_buffer,
                                           const bool single_file);

    void          node_export_to_html(CtTreeIter tree_iter, const CtExportOptions& options, const Glib::ustring& index, int sel_start, int sel_end);
    void          nodes_all_export_to_multiple_html(bool all_tree, const CtExportOptions& options);
    void          nodes_all_export_to_single_html(bool all_tree, const CtExportOptions& options);
    Glib::ustring selection_export_to_html(Glib::RefPtr<Gtk::TextBuffer> text_buffer, Gtk::TextIter start_iter,
                                           Gtk::TextIter end_iter, const Glib::ustring& syntax_highlighting);
    Glib::ustring table_export_to_html(CtTableCommon* table);
    Glib::ustring codebox_export_to_html(CtCodebox* codebox);
    bool          prepare_html_folder(fs::path dir_place, fs::path new_folder, bool export_overwrite, fs::path& export_path);

private:
    Glib::ustring _get_embfile_html(CtImageEmbFile* embfile, CtTreeIter tree_iter, fs::path embed_dir);
    Glib::ustring _get_image_html(CtImage* image,
                                  const fs::path& images_dir,
                                  int& images_count,
                                  CtTreeIter* tree_iter,
                                  const bool single_file);
    Glib::ustring _get_codebox_html(CtCodebox* codebox);
    Glib::ustring _get_table_html(CtTableCommon* table);

    Glib::ustring _html_get_from_code_buffer(const Glib::RefPtr<Gtk::TextBuffer>& code_buffer,
                                             int sel_start,
                                             int sel_end,
                                             const std::string &syntax_highlighting,
                                             const bool from_selection = false);
    void _html_get_from_treestore_node(CtTreeIter node_iter,
                                       int sel_start,
                                       int sel_end,
                                       std::vector<Glib::ustring>& out_slots,
                                       std::vector<CtAnchoredWidget*>& out_widgets,
                                       const bool single_file);
    static int _html_process_list_info_change(Glib::ustring& html,
                                              std::list<CtListType>& nested_list_types,
                                              CtListInfo* pListInfoFrom,
                                              const CtListInfo* pListInfoTo);
    static Glib::ustring _html_text_serialize(CtMainWin* const pCtMainWin, // the unit tests may pass nullptr here!
                                              Gtk::TextIter start_iter,
                                              Gtk::TextIter end_iter,
                                              const CtCurrAttributesMap& curr_attributes,
                                              const bool single_file);
    static std::string _get_href_from_link_prop_val(CtMainWin* const pCtMainWin,
                                                    const Glib::ustring& link_prop_val,
                                                    const bool single_file);
    Glib::ustring _get_object_alignment_string(Glib::ustring alignment);

    void _tree_links_text_iter(CtTreeIter tree_iter, Glib::ustring& tree_links_text, int tree_count_level, bool index_in_page);

    static Glib::ustring _get_html_filename(CtTreeIter tree_iter);

public:
    static std::string link_process_filepath(const std::string& filepath_raw, const std::string& relative_to, const bool forHtml);
    static std::string link_process_folderpath(const std::string& folderpath_raw, const std::string& relative_to, const bool forHtml);

private:
    CtMainWin* const _pCtMainWin;
    const CtConfig* const _pCtConfig;
    fs::path _export_dir;
    fs::path _images_dir;
    fs::path _embed_dir;
    fs::path _res_dir;
};
