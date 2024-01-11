/* SPDX-FileCopyrightText: Copyright 2023 Duncan Overbruck <mail@duncano.de> */
/* SPDX-License-Identifier: BSD-2-Clause */

#ifndef _XBPS_JSON_H_
#define _XBPS_JSON_H_

#include <stdio.h>
#include <stdint.h>

#include <xbps/xbps_array.h>
#include <xbps/xbps_bool.h>
#include <xbps/xbps_dictionary.h>
#include <xbps/xbps_number.h>
#include <xbps/xbps_object.h>
#include <xbps/xbps_string.h>

/** @addtogroup json */
/**@{*/

/**
 * @struct xbps_json_printer xbps/json.h <xbps/json.h>
 * @brief Structure holding state while printing json.
 */
struct xbps_json_printer {
	/**
	 * @var file
	 * @brief Output file to print to.
	 */
	FILE *file;
	/**
	 * @var depth
	 * @brief The current depth inside objects or arrays.
	 */
	unsigned depth;
	/**
	 * @var indent
	 * @brief Number of indent spaces per depth.
	 */
	uint8_t indent;
	/**
	 * @var compact
	 * @brief Compact mode removes unnecessary spaces.
	 */
	bool compact;
};

/**
 * @brief Escape and write the string \a s to the json file.
 *
 * @param[in] p Json print context.
 * @param[in] s The string to write.
 *
 * @return 0 on success or a negative errno from fprintf(3).
 */
int xbps_json_print_escaped(struct xbps_json_printer *p, const char *s);

/**
 * @brief Write the string \a s as quoted string to the json file.
 *
 * @param[in] p Json print context.
 * @param[in] s The string to write.
 *
 * @return 0 on success or a negative errno from fprintf(3).
 */
int xbps_json_print_quoted(struct xbps_json_printer *p, const char *s);

/**
 * @brief Write boolean to the json stream.
 *
 * @param[in] p Json print context.
 * @param[in] b Boolean value.
 *
 * @return 0 on success or a negative errno from fprintf(3).
 */
int xbps_json_print_bool(struct xbps_json_printer *p, bool b);

/**
 * @brief Write a ::xbps_string_t to the json stream.
 *
 * @param[in] p Json print context.
 * @param[in] str String value to print.
 *
 * @return 0 on success or a negative errno from fprintf(3).
 */
int xbps_json_print_xbps_string(struct xbps_json_printer *p, xbps_string_t str);

/**
 * @brief Write a ::xbps_number_t to the json stream.
 *
 * @param[in] p Json print context.
 * @param[in] num Number value to print.
 *
 * @return 0 on success or a negative errno from fprintf(3).
 */
int xbps_json_print_xbps_number(struct xbps_json_printer *p, xbps_number_t num);

/**
 * @brief Write a ::xbps_boolean_t to the json stream.
 *
 * @param[in] p Json print context.
 * @param[in] b Boolean value to print.
 *
 * @return 0 on success or a negative errno from fprintf(3).
 */
int xbps_json_print_xbps_boolean(struct xbps_json_printer *p, xbps_bool_t b);

/**
 * @brief Write a ::xbps_array_t to the json stream.
 *
 * @param[in] p Json print context.
 * @param[in] array Array to print.
 *
 * @return 0 on success or a negative errno from fprintf(3).
 */
int xbps_json_print_xbps_array(struct xbps_json_printer *p, xbps_array_t array);

/**
 * @brief Write a ::xbps_dictionary_t to the json stream.
 *
 * @param[in] p Json print context.
 * @param[in] dict Dictionary to print.
 *
 * @return 0 on success or a negative errno from fprintf(3).
 */
int xbps_json_print_xbps_dictionary(struct xbps_json_printer *p, xbps_dictionary_t dict);

/**
 * @brief Write a ::xbps_object_t to the json stream.
 *
 * @param[in] p Json print context.
 * @param[in] obj Object to print.
 *
 * @return 0 on success or a negative errno from fprintf(3).
 */
int xbps_json_print_xbps_object(struct xbps_json_printer *p, xbps_object_t obj);

/**@}*/

#endif /* !_XBPS_JSON_H_ */
