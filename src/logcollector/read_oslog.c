/* Copyright (C) 2015-2021, Wazuh Inc.
 * All right reserved.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation
 */
#if defined(Darwin) || (defined(__linux__) && defined(WAZUH_UNIT_TESTING))
#include "shared.h"
#include "logcollector.h"

#ifdef WAZUH_UNIT_TESTING
// Remove STATIC qualifier from tests
#define STATIC
#else
#define STATIC static
#endif

#define LOG_ERROR_STR    "log:"
#define LOG_ERROR_LENGHT 4

/**
 * @brief Gets a log from `log stream`
 *
 * @param [out] buffer Contains the read log
 * @param length Buffer's max length
 * @param stream File pointer to log stream's output pipe
 * @param oslog_cfg oslog configuration structure
 * @return  true if a new log was collected,
 *          false otherwise
 */
STATIC bool oslog_getlog(char * buffer, int length, FILE * stream, w_oslog_config_t * oslog_cfg);

/**
 * @brief Restores the context from backup
 *
 * @warning Notice that `buffer` must be previously allocated, the function does
 * not verify nor allocate or release the buffer memory
 * @param buffer Destination buffer
 * @param ctxt Backup context
 * @return true if the context was restored, otherwise returns false
 */
STATIC bool oslog_ctxt_restore(char * buffer, w_oslog_ctxt_t * ctxt);

/**
 * @brief Generates a backup of the reading context
 *
 * @param buffer Context to backup
 * @param ctxt Context's backup destination
 */
STATIC void oslog_ctxt_backup(char * buffer, w_oslog_ctxt_t * ctxt);

/**
 * @brief Cleans the backup context
 *
 * @warning Notice that this function does not release the context memory
 * @param ctxt context backup to clean
 */
STATIC void oslog_ctxt_clean(w_oslog_ctxt_t * ctxt);

/**
 * @brief Checks if a backup context has expired
 *
 * @todo  Remove timeout and use a define
 * @param timeout A timeout that a context without updating is valid
 * @param ctxt Context to check
 * @return true if the context has expired, otherwise returns false
 */
STATIC bool oslog_ctxt_is_expired(time_t timeout, w_oslog_ctxt_t * ctxt);

/**
 * @brief Gets the pointer to the beginning of the last line contained in the string
 *
 * @warning If `str` has one line, returns NULL
 * @warning If `str` ends with a `\n`, this newline character is ignored
 * @param str String to be analyzed
 * @return Pointer to the beginning of the last line, NULL otherwise
 */
STATIC char * oslog_get_valid_lastline(char * str);

/**
 * @brief Checks whether the `log stream` cli command returns a header or a log.
 * 
 * Detects predicate errors and discards filtering headers and columun descriptions.
 * @param oslog_cfg oslog configuration structure
 * @param buffer line to check
 * @return Returns false if the read line is a log, otherwise returns true 
 */
STATIC bool oslog_is_header(w_oslog_config_t * oslog_cfg, char * buffer);

/**
 * @brief Trim milliseconds from a macOS ULS full timestamp
 * 
 * @param full_timestamp Timestamp to trim
 * @warning @full_timestamp must be an array with \ref OS_LOGCOLLECTOR_FULL_TIMESTAMP_LEN +1 length
 * @warning @full_timestamp must be in format i.e 2020-11-09 05:45:08.000000-0800
 * @warning return value will be in short format timestamp i.e 2020-11-09 05:45:08-0800
 * @return Allocated short timestamp. NULL on error
 */
STATIC char * w_oslog_trim_full_timestamp(const char * full_timestamp) {
    char * short_timestamp = NULL;

    if (w_strlen(full_timestamp) == OS_LOGCOLLECTOR_FULL_TIMESTAMP_LEN
        && full_timestamp[OS_LOGCOLLECTOR_FULL_TIMESTAMP_LEN-1] != '\0') {

        os_calloc(OS_LOGCOLLECTOR_SHORT_TIMESTAMP_LEN + 1, sizeof(char), short_timestamp);
        memcpy(short_timestamp, full_timestamp, OS_LOGCOLLECTOR_BASIC_TIMESTAMP_LEN);
        memcpy(short_timestamp + OS_LOGCOLLECTOR_BASIC_TIMESTAMP_LEN,
                full_timestamp + OS_LOGCOLLECTOR_BASIC_TIMESTAMP_LEN + OS_LOGCOLLECTOR_TIMESTAMP_MS_LEN,
                OS_LOGCOLLECTOR_TIMESTAMP_TZ_LEN);
    }

    return short_timestamp;
}

void * read_oslog(logreader * lf, int * rc, int drop_it) {
    char read_buffer[OS_MAXSTR + 1];
    char full_timestamp[OS_LOGCOLLECTOR_FULL_TIMESTAMP_LEN + 1] = {'\0'};
    char * short_timestamp = NULL;

    int count_logs = 0;
    int status = 0;
    int retval = 0;
    const int MAX_LINE_LEN = OS_MAXSTR - OS_LOG_HEADER;

    if (can_read() == 0) {
        return NULL;
    }

    read_buffer[OS_MAXSTR] = '\0';
    *rc = 0;

    while (oslog_getlog(read_buffer, MAX_LINE_LEN, lf->oslog->stream_wfd->file, lf->oslog)
           && (maximum_lines == 0 || count_logs < maximum_lines)) {

        if (drop_it == 0) {
            unsigned long size = strlen(read_buffer);
            if (size > 0) {
                w_msg_hash_queues_push(read_buffer, OSLOG_NAME, size + 1, lf->log_target, LOCALFILE_MQ);
            } else {
                mdebug2("ULS: Discarding empty message...");
            }

        }
        memcpy(full_timestamp, read_buffer, OS_LOGCOLLECTOR_FULL_TIMESTAMP_LEN);
        full_timestamp[OS_LOGCOLLECTOR_FULL_TIMESTAMP_LEN] = '\0';
        count_logs++;
    }

    short_timestamp = w_oslog_trim_full_timestamp(full_timestamp);
    if (short_timestamp != NULL) {
        w_update_oslog_status(short_timestamp);
        os_free(short_timestamp);
    }

    /* Checks whether the OSLog's process is still alive or exited */
    retval = waitpid(lf->oslog->stream_wfd->pid, &status, WNOHANG);    // Tries to get the child' "soul"
    if (retval == lf->oslog->stream_wfd->pid) {                        // This is true in the case that the child exited
        merror(LOGCOLLECTOR_OSLOG_CHILD_ERROR, lf->oslog->stream_wfd->pid, status);
        w_oslog_release();
        lf->oslog->is_oslog_running = false;
    } else if (retval != 0) {
        merror(WAITPID_ERROR, errno, strerror(errno));
    }

    return NULL;
}

STATIC bool oslog_getlog(char * buffer, int length, FILE * stream, w_oslog_config_t * oslog_cfg) {

    bool retval = false; // This variable will be set to true if there is a buffered log

    int offset = 0;          // Amount of chars in the buffer
    char * str = buffer;     // Auxiliar buffer pointer, it points where the new data will be stored
    int chunk_sz = 0;        // Size of the last read data
    char * last_line = NULL; // Pointer to the last line stored in the buffer
    bool is_buffer_full;     // Will be set to true if the buffer is full (forces data to be sent)
    bool is_endline;         // Will be set to true if the last read line ends with an '\n' character
    bool do_split;           // Indicates whether the buffer will be splited (two chunks at most)

    *str = '\0';

    /* Checks if a context recover is needed for incomplete logs */
    if (oslog_ctxt_restore(str, &oslog_cfg->ctxt)) {
        offset = strlen(str);

        /* If the context is expired then frees it and returns the log */
        if (oslog_ctxt_is_expired((time_t) OSLOG_TIMEOUT, &oslog_cfg->ctxt)) {
            oslog_ctxt_clean(&oslog_cfg->ctxt);
            /* delete last end-of-line character */
            if (buffer[offset - 1] == '\n') {
                buffer[offset - 1] = '\0';
            }
            return true;
        }

        str += offset;
    }

    /* Gets streamed data, the minimum chunk size of a log is one line */
    while (can_read() && (fgets(str, length - offset, stream) != NULL)) {

        chunk_sz = strlen(str);
        offset += chunk_sz;
        str += chunk_sz;
        last_line = NULL;
        is_buffer_full = false;
        is_endline = (*(str - 1) == '\n');
        do_split = false;

        /* Avoid fgets infinite loop behavior when size parameter is 1
         * If we didn't get the new line, because the size is large, send what we got so far.
         */
        if (offset + 1 == length) {
            // Cleans the context and forces to send a log
            oslog_ctxt_clean(&oslog_cfg->ctxt);
            is_buffer_full = true;
        } else if (!is_endline) {
            mdebug2("ULS: Inclomplete message...");
            // Saves the context
            oslog_ctxt_backup(buffer, &oslog_cfg->ctxt);
            continue;
        }

        /* Checks if the first line is the header or an error in the predicate. */
        if (!oslog_cfg->is_header_processed) {
            /* Processes and discards lines up to the first log */
            if (oslog_is_header(oslog_cfg, buffer)) {
                // Forces to continue reading
                retval = true;
                *buffer = '\0';
                break;
            }
        }

        /* If this point has been reached, there is something to process in the buffer. */

        last_line = oslog_get_valid_lastline(buffer);

        /* If there are 2 logs, they should be splited before sending them */
        if (is_endline && last_line != NULL) {
            do_split = w_expression_match(oslog_cfg->start_log_regex, last_line + 1, NULL, NULL);
        }

        if (!do_split && is_buffer_full) {
            /* If the buffer is full but the message is larger than the buffer size, 
             * then the rest of the message is discarded up to the '\n' character.
             */
            if (!is_endline) {
                if (last_line == NULL) {
                    char c;
                    // Discards the rest of the log, up to the end of line
                    do {
                        c = fgetc(stream);
                    } while (c != '\n' && c != '\0' && c != EOF);
                    mdebug2("Max oslog message length reached... The rest of the message was discarded");
                } else {
                    do_split = true;
                    mdebug2("Max oslog message length reached... The rest of the message will be send separately");
                }
            }
        }

        /* splits the logs */
        /* If a new log is received, we store it in the context and send the previous one. */
        if (do_split) {
            oslog_ctxt_clean(&oslog_cfg->ctxt);
            *last_line = '\0';
            strncpy(oslog_cfg->ctxt.buffer, last_line + 1, offset - (last_line - buffer) + 1);
            oslog_cfg->ctxt.timestamp = time(NULL);
        } else if (!is_buffer_full) {
            oslog_ctxt_backup(buffer, &oslog_cfg->ctxt);
        }

        if (do_split || is_buffer_full) {
            retval = true;
            /* deletes last end-of-line character  */
            if (buffer[offset - 1] == '\n') {
                buffer[offset - 1] = '\0';
            }
            break;
        }
    }

    return retval;
}

STATIC bool oslog_ctxt_restore(char * buffer, w_oslog_ctxt_t * ctxt) {

    if (ctxt->buffer[0] == '\0') {
        return false;
    }

    strcpy(buffer, ctxt->buffer);
    return true;
}

STATIC bool oslog_ctxt_is_expired(time_t timeout, w_oslog_ctxt_t * ctxt) {

    if (time(NULL) - ctxt->timestamp > timeout) {
        return true;
    }

    return false;
}

STATIC void oslog_ctxt_clean(w_oslog_ctxt_t * ctxt) {

    ctxt->buffer[0] = '\0';
    ctxt->timestamp = 0;
}

STATIC void oslog_ctxt_backup(char * buffer, w_oslog_ctxt_t * ctxt) {

    /* Backup */
    strcpy(ctxt->buffer, buffer);
    ctxt->timestamp = time(NULL);
}

STATIC char * oslog_get_valid_lastline(char * str) {

    char * retval = NULL;
    char ignored_char = '\0';
    size_t size = 0;

    if (str == NULL || *str == '\0') {
        return retval;
    }

    /* Ignores the last character */
    size = strlen(str);

    ignored_char = str[size - 1];
    str[size - 1] = '\0';

    retval = strrchr(str, '\n');
    str[size - 1] = ignored_char;

    return retval;
}

STATIC bool oslog_is_header(w_oslog_config_t * oslog_cfg, char * buffer) {

    bool retval = true;
    const ssize_t buffer_size = strlen(buffer);

    /* if the buffer contains a log, then there won't be headers anymore */
    if (w_expression_match(oslog_cfg->start_log_regex, buffer, NULL, NULL)) {
        oslog_cfg->is_header_processed = true;
        retval = false;
    }
    /* Error in the execution of the `log stream` cli command, probably there is an error in the predicate. */
    else if (strncmp(buffer, LOG_ERROR_STR, LOG_ERROR_LENGHT) == 0) {

        // "log: error description:\n"
        if (buffer[buffer_size - 2] == ':') {
            buffer[buffer_size - 2] = '\0';
        } else if (buffer[buffer_size - 1] == '\n') {
            buffer[buffer_size - 1] = '\0';
        }
        merror(LOGCOLLECTOR_OSLOG_ERROR_AFTER_EXEC, buffer);
    }
    /* Rows header or remaining error lines */
    else {
        if (buffer[buffer_size - 1] == '\n') {
            buffer[buffer_size - 1] = '\0';
        }
        mdebug2("Reading other log headers or errors: '%s'", buffer);
    }

    return retval;
}

#endif
