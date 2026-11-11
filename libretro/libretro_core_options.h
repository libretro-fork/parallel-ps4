/*
 * shadPS4 libretro core - Core Options v2
 *
 * The English texts are edited here; the translations in
 * libretro_core_options_intl.h come from Crowdin (intl/).
 *
 * SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef LIBRETRO_CORE_OPTIONS_H__
#define LIBRETRO_CORE_OPTIONS_H__

#include <stdlib.h>
#include <string.h>

#include "libretro.h"

#ifndef HAVE_NO_LANGEXTRA
#include "libretro_core_options_intl.h"
#endif

/*
 ********************************
 * VERSION: 2.0
 ********************************
 *
 * - 2.0: Add support for core options v2 interface
 * - 1.3: Move translations to libretro_core_options_intl.h
 *        - libretro_core_options_intl.h includes BOM and utf-8
 *          fix for MSVC 2010-2013
 *        - Added HAVE_NO_LANGEXTRA flag to disable translations
 *          on platforms/compilers without BOM support
 * - 1.2: Use core options v1 interface when
 *        RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION is >= 1
 *        (previously required RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION == 1)
 * - 1.1: Support generation of core options v0 retro_core_option_value
 *        arrays containing options with a single value
 * - 1.0: First commit
*/

#ifdef __cplusplus
extern "C" {
#endif

/*
 ********************************
 * Core Option Definitions
/*
 ********************************
 * Core Option Definitions
 ********************************
*/

/* RETRO_LANGUAGE_ENGLISH */

/* Default language:
 * - All other languages must include the same keys and values
 * - Will be used as a fallback in the event that frontend language
 *   is not available
 * - Will be used as a fallback for any missing entries in
 *   frontend language definition
 * - Translations live in libretro_core_options_intl.h, which is
 *   generated from Crowdin - edit only the English texts here
 */

struct retro_core_option_v2_category option_cats_us[] = {
    {
        "system",
        "System",
        "Settings of the emulated console."
    },
    {
        "video",
        "Video",
        "Settings of the picture: its size, upscaling and frame timing."
    },
    {
        "audio",
        "Audio",
        "Settings of the sound."
    },
    {
        "trophies",
        "Trophies",
        "Settings of the trophy notifications."
    },
    {
        "logging",
        "Logging",
        "Settings of the emulator's log."
    },
    { NULL, NULL, NULL },
};

/* Each option sets the emulator's setting of the same meaning when a game
 * starts (libretro_core.cpp, ApplyCoreOptions); the defaults are the
 * emulator's own. They take effect the next time a game is started. */
struct retro_core_option_v2_definition option_defs_us[] = {
    {
        "shadps4_neo_mode",
        "PS4 Pro Mode",
        NULL,
        "Runs the game as on a PS4 Pro. Games with Pro support may render at a higher resolution or frame rate; some games are less stable with it.",
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_console_language",
        "Console Language",
        NULL,
        "The system language the console reports to the game. Games that have the language use it for their texts and voices.",
        NULL,
        "system",
        {
            { "1", "English (United States)" },
            { "18", "English (United Kingdom)" },
            { "21", "Arabic" },
            { "23", "Czech" },
            { "14", "Danish" },
            { "6", "Dutch" },
            { "12", "Finnish" },
            { "22", "French (Canada)" },
            { "2", "French (France)" },
            { "4", "German" },
            { "25", "Greek" },
            { "24", "Hungarian" },
            { "29", "Indonesian" },
            { "5", "Italian" },
            { "0", "Japanese" },
            { "9", "Korean" },
            { "15", "Norwegian (Bokmaal)" },
            { "16", "Polish" },
            { "17", "Portuguese (Brazil)" },
            { "7", "Portuguese (Portugal)" },
            { "26", "Romanian" },
            { "8", "Russian" },
            { "11", "Simplified Chinese" },
            { "20", "Spanish (Latin America)" },
            { "3", "Spanish (Spain)" },
            { "13", "Swedish" },
            { "27", "Thai" },
            { "10", "Traditional Chinese" },
            { "19", "Turkish" },
            { "30", "Ukrainian" },
            { "28", "Vietnamese" },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_dev_kit",
        "Devkit Console Mode",
        NULL,
        "Runs the game as on a development kit, which has more memory than a retail console. A few games and most homebrew debugging tools need it.",
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_extra_memory",
        "Additional Memory",
        NULL,
        "Gives the game more memory than the console has. Helps games that run out of memory under emulation; leave at none otherwise.",
        NULL,
        "system",
        {
            { "0", "None" },
            { "512", "512 MB" },
            { "1024", "1 GB" },
            { "2048", "2 GB" },
            { "3072", "3 GB" },
            { "4096", "4 GB" },
            { "6144", "6 GB" },
            { "8192", "8 GB" },
            { NULL, NULL },
        },
        "0"
    },
    {
        "shadps4_confirm_button",
        "Confirm Button",
        NULL,
        "The button system dialogs take as confirm. Cross is the Western layout, Circle the Japanese one.",
        NULL,
        "system",
        {
            { "cross",  "Cross" },
            { "circle", "Circle" },
            { NULL, NULL },
        },
        "cross"
    },
    {
        "shadps4_network",
        "Connected to Network",
        NULL,
        "Tells the game the console is connected to a network. Some games need it to start, others wait for online services that are not there.",
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_pkg_content",
        "PKG Content",
        NULL,
        "How a game loaded as a PKG is run. Run from PKG reads its files straight out of the PKG and needs no room on the disk. Install extracts the game, its update and its additional content into the system folder the first time, and runs it from there, which reads faster on a slow drive or machine but takes the room of the whole game.",
        NULL,
        "system",
        {
            { "run",     "Run from PKG" },
            { "install", "Install" },
            { NULL, NULL },
        },
        "run"
    },
    {
        "shadps4_controllers",
        "Controllers",
        NULL,
        "How many controllers are connected to the console, from the frontend's ports 1 to 4. Each one logs in the user of that player number, as plugging a controller in does; some games change how they start when more than one is connected.",
        NULL,
        "system",
        {
            { "1", NULL },
            { "2", NULL },
            { "3", NULL },
            { "4", NULL },
            { NULL, NULL },
        },
        "1"
    },
    {
        "shadps4_show_splash",
        "Show Splash Screen",
        NULL,
        "Shows the game's splash picture while it starts.",
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_game_resolution",
        "Resolution",
        NULL,
        "The size of the picture, and of the game's rendering. Default: the game renders at its own resolution and the picture is that size. Below the game's own, the game renders at the chosen size, which is much lighter on a slow GPU; effects that expect the full size can come out wrong. Above it, the game renders at its own and the picture is scaled up. Requires restart.",
        NULL,
        "video",
        {
            { "default",   "Default (game's resolution)" },
            { "320x180",   "320x180" },
            { "480x270",   "480x270" },
            { "640x360",   "640x360" },
            { "800x450",   "800x450" },
            { "960x540",   "960x540" },
            { "1280x720",  "1280x720 (720p)" },
            { "1920x1080", "1920x1080 (1080p)" },
            { "2560x1440", "2560x1440 (1440p)" },
            { "3840x2160", "3840x2160 (4K)" },
            { NULL, NULL },
        },
        "default"
    },
    {
        "shadps4_vblank_frequency",
        "VBlank Frequency",
        NULL,
        "The refresh rate of the emulated console. 60 Hz is the real one. Higher values make some games run faster or at a higher frame rate, and break others.",
        NULL,
        "video",
        {
            { "30",  "30 Hz" },
            { "60",  "60 Hz" },
            { "90",  "90 Hz" },
            { "120", "120 Hz" },
            { "144", "144 Hz" },
            { "240", "240 Hz" },
            { NULL, NULL },
        },
        "60"
    },
    {
        "shadps4_readbacks",
        "GPU Readbacks",
        NULL,
        "Copies what the GPU drew back to the game's memory. Some games need it for correct graphics; it costs speed. Precise is slower and more exact than Relaxed.",
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "relaxed",  "Relaxed" },
            { "precise",  "Precise" },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_direct_memory_access",
        "Direct Memory Access",
        NULL,
        "Lets shaders read the game's memory directly. Fixes graphics in some games and can crash others.",
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_readback_linear_images",
        "Readback Linear Images",
        NULL,
        "Copies images the GPU drew back to the game's memory when the game reads them. Fixes graphics in some games; costs speed.",
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        // On by default, unlike the standalone's setting. The core's picture reaches the
        // frontend a frame or two after the game flipped it, so a game's command buffer
        // regularly waits in the GPU thread while the game already builds its next frame in
        // the same memory, and the flip that follows is for the wrong buffer ("Out of order
        // flip IRQ", Sonic Mania on entering Mania Mode).
        "shadps4_copy_gpu_buffers",
        "Copy GPU Buffers",
        NULL,
        "Copies a game's command buffers when it submits them, so the game cannot write its next frame over one the emulator has not drawn yet. Without it some games stop with an out of order flip. Costs a little speed.",
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
#if defined(__aarch64__) && !defined(SHADPS4_ENABLE_FEX_GUEST_CPU)
    {
        // On by default: without it Sonic Mania's music, which the game decodes itself, was
        // silent while its sound effects played. Only the ARM build translates the emulator.
        "shadps4_exact_flags",
        "Exact CPU Flags",
        NULL,
        "On ARM the emulator's x86-64 code is translated as it runs. With this on, the translation works out the CPU's flags exactly, which some games need: without it Sonic Mania's music stays silent. Off is faster. Takes effect when the game starts.",
        NULL,
        "system",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
#endif
    {
        "shadps4_shader_cache",
        "Shader Cache",
        NULL,
        "Keeps compiled shader pipelines on disk, so a game stutters less the next time it is started.",
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_vertex_capture",
        "Capture Vertex Inputs (diagnostics)",
        NULL,
        "Switching this on writes the vertex inputs of about one frame's draws to vertex_capture.bin in the log directory, for tools/vertex_capture_report: do it while a mesh is drawn broken, then switch it off again. Switching it on again captures anew.",
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_dump_shaders",
        "Dump Shaders (diagnostics)",
        NULL,
        "Writes every shader the game uses, as the game has it (.bin) and as it is given to the GPU (.spv), to shadPS4/shader/dumps under the frontend's system directory. For looking into a picture that is wrong; takes effect when a game is started.",
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_show_fps",
        "Show FPS Counter",
        NULL,
        "Draws the emulator's own frame rate counter into the picture.",
        NULL,
        "video",
        {
            { "disabled", NULL },
            { "enabled",  NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_volume",
        "Volume",
        NULL,
        "The volume of the game's sound, before the frontend's own volume. Above 100% the sound is amplified and can distort.",
        NULL,
        "audio",
        {
            { "0",   "0%" },
            { "10",  "10%" },
            { "20",  "20%" },
            { "30",  "30%" },
            { "40",  "40%" },
            { "50",  "50%" },
            { "60",  "60%" },
            { "70",  "70%" },
            { "80",  "80%" },
            { "90",  "90%" },
            { "100", "100%" },
            { "125", "125%" },
            { "150", "150%" },
            { "200", "200%" },
            { "300", "300%" },
            { "400", "400%" },
            { "500", "500%" },
            { NULL, NULL },
        },
        "100"
    },
    {
        "shadps4_trophy_notifications",
        "Trophy Notifications",
        NULL,
        "Shows a notification in the picture when a trophy is earned.",
        NULL,
        "trophies",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_trophy_position",
        "Trophy Notification Position",
        NULL,
        "The side of the picture trophy notifications appear on.",
        NULL,
        "trophies",
        {
            { "left", "Left" },
            { "right", "Right" },
            { "top", "Top" },
            { "bottom", "Bottom" },
            { NULL, NULL },
        },
        "right"
    },
    {
        "shadps4_trophy_duration",
        "Trophy Notification Duration",
        NULL,
        "How long a trophy notification stays in the picture.",
        NULL,
        "trophies",
        {
            { "1", "1 s" },
            { "2", "2 s" },
            { "3", "3 s" },
            { "4", "4 s" },
            { "5", "5 s" },
            { "6", "6 s" },
            { "7", "7 s" },
            { "8", "8 s" },
            { "9", "9 s" },
            { "10", "10 s" },
            { NULL, NULL },
        },
        "6"
    },
    {
        "shadps4_log",
        "Logging",
        NULL,
        "Writes the emulator's log to system/shadPS4/log/. Turning it off can gain a little speed; a log is needed to report a problem.",
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    {
        "shadps4_log_separate",
        "Separate Log Files",
        NULL,
        "Writes each game's log to a file of its own, named after the game's ID, instead of one shadps4.log.",
        NULL,
        "logging",
        {
            { "disabled", NULL },
            { "enabled", NULL },
            { NULL, NULL },
        },
        "disabled"
    },
    {
        "shadps4_log_sync",
        "Log Sync",
        NULL,
        "Writes every log line at once, so nothing is missing after a crash. Disabled, logging is done in the background, which is faster.",
        NULL,
        "logging",
        {
            { "enabled", NULL },
            { "disabled", NULL },
            { NULL, NULL },
        },
        "enabled"
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};

struct retro_core_options_v2 options_us = {
   option_cats_us,
   option_defs_us
};

/*
 ********************************
 * Language Mapping
 ********************************
*/

#ifndef HAVE_NO_LANGEXTRA
struct retro_core_options_v2 *options_intl[RETRO_LANGUAGE_LAST] = {
   &options_us,       /* RETRO_LANGUAGE_ENGLISH */
   &options_ja,       /* RETRO_LANGUAGE_JAPANESE */
   &options_fr,       /* RETRO_LANGUAGE_FRENCH */
   &options_es,       /* RETRO_LANGUAGE_SPANISH */
   &options_de,       /* RETRO_LANGUAGE_GERMAN */
   &options_it,       /* RETRO_LANGUAGE_ITALIAN */
   &options_nl,       /* RETRO_LANGUAGE_DUTCH */
   &options_pt_br,    /* RETRO_LANGUAGE_PORTUGUESE_BRAZIL */
   &options_pt_pt,    /* RETRO_LANGUAGE_PORTUGUESE_PORTUGAL */
   &options_ru,       /* RETRO_LANGUAGE_RUSSIAN */
   &options_ko,       /* RETRO_LANGUAGE_KOREAN */
   &options_cht,      /* RETRO_LANGUAGE_CHINESE_TRADITIONAL */
   &options_chs,      /* RETRO_LANGUAGE_CHINESE_SIMPLIFIED */
   &options_eo,       /* RETRO_LANGUAGE_ESPERANTO */
   &options_pl,       /* RETRO_LANGUAGE_POLISH */
   &options_vn,       /* RETRO_LANGUAGE_VIETNAMESE */
   &options_ar,       /* RETRO_LANGUAGE_ARABIC */
   &options_el,       /* RETRO_LANGUAGE_GREEK */
   &options_tr,       /* RETRO_LANGUAGE_TURKISH */
   &options_sk,       /* RETRO_LANGUAGE_SLOVAK */
   &options_fa,       /* RETRO_LANGUAGE_PERSIAN */
   &options_he,       /* RETRO_LANGUAGE_HEBREW */
   &options_ast,      /* RETRO_LANGUAGE_ASTURIAN */
   &options_fi,       /* RETRO_LANGUAGE_FINNISH */
   &options_id,       /* RETRO_LANGUAGE_INDONESIAN */
   &options_sv,       /* RETRO_LANGUAGE_SWEDISH */
   &options_uk,       /* RETRO_LANGUAGE_UKRAINIAN */
   &options_cs,       /* RETRO_LANGUAGE_CZECH */
   &options_val,      /* RETRO_LANGUAGE_CATALAN_VALENCIA */
   &options_ca,       /* RETRO_LANGUAGE_CATALAN */
   &options_en,       /* RETRO_LANGUAGE_BRITISH_ENGLISH */
   &options_hu,       /* RETRO_LANGUAGE_HUNGARIAN */
   &options_be,       /* RETRO_LANGUAGE_BELARUSIAN */
   &options_gl,       /* RETRO_LANGUAGE_GALICIAN */
   &options_no,       /* RETRO_LANGUAGE_NORWEGIAN */
   &options_ga,       /* RETRO_LANGUAGE_IRISH */
};
#endif

/*
 ********************************
 * Functions
 ********************************
*/

/* Handles configuration/setting of core options.
 * Should be called as early as possible - ideally inside
 * retro_set_environment(), and no later than retro_load_game()
 * > We place the function body in the header to avoid the
 *   necessity of adding more .c files (i.e. want this to
 *   be as painless as possible for core devs)
 */

static inline void libretro_set_core_options(retro_environment_t environ_cb,
      bool *categories_supported)
{
   unsigned version  = 0;
#ifndef HAVE_NO_LANGEXTRA
   unsigned language = 0;
#endif

   if (!environ_cb || !categories_supported)
      return;

   *categories_supported = false;

   if (!environ_cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version))
      version = 0;

   if (version >= 2)
   {
#ifndef HAVE_NO_LANGEXTRA
      struct retro_core_options_v2_intl core_options_intl;

      core_options_intl.us    = &options_us;
      core_options_intl.local = NULL;

      if (environ_cb(RETRO_ENVIRONMENT_GET_LANGUAGE, &language) &&
          (language < RETRO_LANGUAGE_LAST) && (language != RETRO_LANGUAGE_ENGLISH))
         core_options_intl.local = options_intl[language];

      *categories_supported = environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL,
            &core_options_intl);
#else
      *categories_supported = environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2,
            &options_us);
#endif
   }
   else
   {
      size_t i, j;
      size_t option_index              = 0;
      size_t num_options               = 0;
      struct retro_core_option_definition
            *option_v1_defs_us         = NULL;
#ifndef HAVE_NO_LANGEXTRA
      size_t num_options_intl          = 0;
      struct retro_core_option_v2_definition
            *option_defs_intl          = NULL;
      struct retro_core_option_definition
            *option_v1_defs_intl       = NULL;
      struct retro_core_options_intl
            core_options_v1_intl;
#endif
      struct retro_variable *variables = NULL;
      char **values_buf                = NULL;

      /* Determine total number of options */
      while (true)
      {
         if (option_defs_us[num_options].key)
            num_options++;
         else
            break;
      }

      if (version >= 1)
      {
         /* Allocate US array */
         option_v1_defs_us = (struct retro_core_option_definition *)
               calloc(num_options + 1, sizeof(struct retro_core_option_definition));

         /* Copy parameters from option_defs_us array */
         for (i = 0; i < num_options; i++)
         {
            struct retro_core_option_v2_definition *option_def_us = &option_defs_us[i];
            struct retro_core_option_value *option_values         = option_def_us->values;
            struct retro_core_option_definition *option_v1_def_us = &option_v1_defs_us[i];
            struct retro_core_option_value *option_v1_values      = option_v1_def_us->values;

            option_v1_def_us->key           = option_def_us->key;
            option_v1_def_us->desc          = option_def_us->desc;
            option_v1_def_us->info          = option_def_us->info;
            option_v1_def_us->default_value = option_def_us->default_value;

            /* Values must be copied individually... */
            while (option_values->value)
            {
               option_v1_values->value = option_values->value;
               option_v1_values->label = option_values->label;

               option_values++;
               option_v1_values++;
            }
         }

#ifndef HAVE_NO_LANGEXTRA
         if (environ_cb(RETRO_ENVIRONMENT_GET_LANGUAGE, &language) &&
             (language < RETRO_LANGUAGE_LAST) && (language != RETRO_LANGUAGE_ENGLISH) &&
             options_intl[language])
            option_defs_intl = options_intl[language]->definitions;

         if (option_defs_intl)
         {
            /* Determine number of intl options */
            while (true)
            {
               if (option_defs_intl[num_options_intl].key)
                  num_options_intl++;
               else
                  break;
            }

            /* Allocate intl array */
            option_v1_defs_intl = (struct retro_core_option_definition *)
                  calloc(num_options_intl + 1, sizeof(struct retro_core_option_definition));

            /* Copy parameters from option_defs_intl array */
            for (i = 0; i < num_options_intl; i++)
            {
               struct retro_core_option_v2_definition *option_def_intl = &option_defs_intl[i];
               struct retro_core_option_value *option_values           = option_def_intl->values;
               struct retro_core_option_definition *option_v1_def_intl = &option_v1_defs_intl[i];
               struct retro_core_option_value *option_v1_values        = option_v1_def_intl->values;

               option_v1_def_intl->key           = option_def_intl->key;
               option_v1_def_intl->desc          = option_def_intl->desc;
               option_v1_def_intl->info          = option_def_intl->info;
               option_v1_def_intl->default_value = option_def_intl->default_value;

               /* Values must be copied individually... */
               while (option_values->value)
               {
                  option_v1_values->value = option_values->value;
                  option_v1_values->label = option_values->label;

                  option_values++;
                  option_v1_values++;
               }
            }
         }

         core_options_v1_intl.us    = option_v1_defs_us;
         core_options_v1_intl.local = option_v1_defs_intl;

         environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL, &core_options_v1_intl);
#else
         environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS, option_v1_defs_us);
#endif
      }
      else
      {
         /* Allocate arrays */
         variables  = (struct retro_variable *)calloc(num_options + 1,
               sizeof(struct retro_variable));
         values_buf = (char **)calloc(num_options, sizeof(char *));

         if (!variables || !values_buf)
            goto error;

         /* Copy parameters from option_defs_us array */
         for (i = 0; i < num_options; i++)
         {
            const char *key                        = option_defs_us[i].key;
            const char *desc                       = option_defs_us[i].desc;
            const char *default_value              = option_defs_us[i].default_value;
            struct retro_core_option_value *values = option_defs_us[i].values;
            size_t buf_len                         = 3;
            size_t default_index                   = 0;

            values_buf[i] = NULL;

            if (desc)
            {
               size_t num_values = 0;

               /* Determine number of values */
               while (true)
               {
                  if (values[num_values].value)
                  {
                     /* Check if this is the default value */
                     if (default_value)
                        if (strcmp(values[num_values].value, default_value) == 0)
                           default_index = num_values;

                     buf_len += strlen(values[num_values].value);
                     num_values++;
                  }
                  else
                     break;
               }

               /* Build values string */
               if (num_values > 0)
               {
                  buf_len += num_values - 1;
                  buf_len += strlen(desc);

                  values_buf[i] = (char *)calloc(buf_len, sizeof(char));
                  if (!values_buf[i])
                     goto error;

                  strcpy(values_buf[i], desc);
                  strcat(values_buf[i], "; ");

                  /* Default value goes first */
                  strcat(values_buf[i], values[default_index].value);

                  /* Add remaining values */
                  for (j = 0; j < num_values; j++)
                  {
                     if (j != default_index)
                     {
                        strcat(values_buf[i], "|");
                        strcat(values_buf[i], values[j].value);
                     }
                  }
               }
            }

            variables[option_index].key   = key;
            variables[option_index].value = values_buf[i];
            option_index++;
         }

         /* Set variables */
         environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);
      }

error:
      /* Clean up */

      if (option_v1_defs_us)
      {
         free(option_v1_defs_us);
         option_v1_defs_us = NULL;
      }

#ifndef HAVE_NO_LANGEXTRA
      if (option_v1_defs_intl)
      {
         free(option_v1_defs_intl);
         option_v1_defs_intl = NULL;
      }
#endif

      if (values_buf)
      {
         for (i = 0; i < num_options; i++)
         {
            if (values_buf[i])
            {
               free(values_buf[i]);
               values_buf[i] = NULL;
            }
         }

         free(values_buf);
         values_buf = NULL;
      }

      if (variables)
      {
         free(variables);
         variables = NULL;
      }
   }
}

#ifdef __cplusplus
}
#endif

#endif
