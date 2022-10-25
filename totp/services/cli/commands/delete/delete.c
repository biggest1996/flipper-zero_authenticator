#include "delete.h"

#include <stdlib.h>
#include <ctype.h>
#include <lib/toolbox/args.h>
#include "../../../list/list.h"
#include "../../../config/config.h"
#include "../../cli_common_helpers.h"
#include "../../../../scenes/scene_director.h"

#define TOTP_CLI_COMMAND_DELETE_ARG_INDEX "INDEX"
#define TOTP_CLI_COMMAND_DELETE_ARG_FORCE_SUFFIX "-f"

void totp_cli_command_delete_print_help() {
    TOTP_CLI_PRINTF("\t" TOTP_CLI_COMMAND_DELETE " " TOTP_CLI_ARG(TOTP_CLI_COMMAND_DELETE_ARG_INDEX) " " TOTP_CLI_OPTIONAL_PARAM(TOTP_CLI_COMMAND_DELETE_ARG_FORCE_SUFFIX) " - delete token\r\n");
    TOTP_CLI_PRINTF("\t\t" TOTP_CLI_ARG(TOTP_CLI_COMMAND_DELETE_ARG_INDEX) " - token index in the list\r\n");
    TOTP_CLI_PRINTF("\t\t" TOTP_CLI_COMMAND_DELETE_ARG_FORCE_SUFFIX " - " TOTP_CLI_OPTIONAL_PARAM_MARK " force command to do not ask user for interactive confirmation\r\n\r\n");
}

void totp_cli_command_delete_handle(PluginState* plugin_state, FuriString* args, Cli* cli) {
    int token_number;
    if (!args_read_int_and_trim(args, &token_number) || token_number <= 0 || token_number > plugin_state->tokens_count) {
        totp_cli_print_invalid_arguments();
        return;
    }

    FuriString* temp_str = furi_string_alloc();
    bool confirm_needed = true;
    if (args_read_string_and_trim(args, temp_str)) {
        if (furi_string_cmpi_str(temp_str, TOTP_CLI_COMMAND_DELETE_ARG_FORCE_SUFFIX) == 0) {
            confirm_needed = false;
        } else {
            TOTP_CLI_PRINTF("Unknown argument \"%s\"\r\n", furi_string_get_cstr(temp_str));
            totp_cli_print_invalid_arguments();
            furi_string_free(temp_str);
            return;
        }
    }
    furi_string_free(temp_str);

    ListNode* list_node = list_element_at(plugin_state->tokens_list, token_number - 1);

    TokenInfo* token_info = list_node->data;

    bool confirmed = !confirm_needed;
    if (confirm_needed) {
        TOTP_CLI_PRINTF("WARNING!\r\n");
        TOTP_CLI_PRINTF("TOKEN \"%s\" WILL BE PERMANENTLY DELETED WITHOUT ABILITY TO RECOVER IT.\r\n", token_info->name);
        TOTP_CLI_PRINTF("Confirm? [y/n]\r\n");
        fflush(stdout);
        char user_pick;
        do {
            user_pick = tolower(cli_getc(cli));
        } while (user_pick != 'y' && user_pick != 'n' && user_pick != CliSymbolAsciiCR);

        confirmed = user_pick == 'y' || user_pick == CliSymbolAsciiCR;
    }

    if (confirmed) {
        bool activate_generate_token_scene = false;
        if (plugin_state->current_scene != TotpSceneAuthentication) {
            totp_scene_director_activate_scene(plugin_state, TotpSceneNone, NULL);
            activate_generate_token_scene = true;
        }
        
        plugin_state->tokens_list = list_remove(plugin_state->tokens_list, list_node);
        plugin_state->tokens_count--;

        totp_full_save_config_file(plugin_state);
        
        if (activate_generate_token_scene) {
            totp_scene_director_activate_scene(plugin_state, TotpSceneGenerateToken, NULL);
        }

        TOTP_CLI_PRINTF("Token \"%s\" has been successfully deleted\r\n", token_info->name);
        token_info_free(token_info);
    } else {
        TOTP_CLI_PRINTF("User not confirmed\r\n");
    }
}