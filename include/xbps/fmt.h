/* SPDX-FileCopyrightText: Copyright 2023 Duncan Overbruck <mail@duncano.de> */
/* SPDX-License-Identifier: BSD-2-Clause */

#ifndef _XBPS_FMT_H_
#define _XBPS_FMT_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <xbps/xbps_dictionary.h>
#include <xbps/xbps_object.h>

/** @addtogroup format */
/**@{*/

/**
 * @struct xbps_fmt xbps/fmt.h <xbps/xbps.h>
 * @brief Structure of parsed format string.
 */
struct xbps_fmt;

/**
 * @struct xbps_fmt xbps/fmt.h <xbps/xbps.h>
 * @brief Structure of a parsed format string variable.
 */
struct xbps_fmt_var {
	/**
	 * @var name
	 * @brief Variable name.
	 */
	char *name;
	/**
	 * @var def
	 * @brief Default value.
	 */
	struct xbps_fmt_def *def;
	/**
	 * @var conv
	 * @brief Format conversion.
	 */
	struct xbps_fmt_conv *conv;
	/**
	 * @var spec
	 * @brief Format specification.
	 */
	struct xbps_fmt_spec *spec;
};

/**
 * @struct xbps_fmt_def xbps/fmt.h <xbps/xbps.h>
 * @brief Structure of format default value.
 */
struct xbps_fmt_def {
	enum {
		XBPS_FMT_DEF_STR = 1,
		XBPS_FMT_DEF_NUM,
		XBPS_FMT_DEF_BOOL,
	} type;
	union {
		char *str;
		int64_t num;
		bool boolean;
	} val;
};

/**
 * @struct xbps_fmt_spec xbps/fmt.h <xbps/xbps.h>
 * @brief Structure of parsed format specifier.
 */
struct xbps_fmt_spec {
	/**
	 * @var fill
	 * @brief Padding character.
	 */
	char fill;
	/**
	 * @var align
	 * @brief Alignment modifier.
	 *
	 * Possible values are:
	 * - `<`: left align.
	 * - `>`: right align.
	 * - `=`: place padding after the sign.
	 */
	char align;
	/**
	 * @var sign
	 * @brief Sign modifier.
	 *
	 * Possible values are:
	 * - `-`: sign negative numbers.
	 * - `+`: sign both negative and positive numbers.
	 * - space: sign negative numbers and add space before positive numbers.
	 */
	char sign;
	/**
	 * @var width
	 * @brief Minimum width.
	 */
	unsigned int width;
	/**
	 * @var precision
	 * @brief Precision.
	 */
	unsigned int precision;
	/**
	 * @var type
	 * @brief Type specifier usually to change the output format type.
	 *
	 * Can contain any character, xbps_fmt_number() uses the following:
	 * - `u`: Unsigned decimal.
	 * - `d`: Decimal.
	 * - `x`: Hex with lowercase letters.
	 * - `X`: hex with uppercase letters.
	 * - `h`: Human readable using humanize_number(3).
	 */
	char type;
};

/**
 * @brief Format callback, called for each variable in the format string.
 *
 * The callback function should write data as specified by \a var to \a fp.
 *
 * @param[in] fp File to format to.
 * @param[in] var Variable to format.
 * @param[in] data Userdata passed to the xbps_fmt() function.
 */
typedef int (xbps_fmt_cb)(FILE *fp, const struct xbps_fmt_var *var, void *data);

/**
 * @brief Parses the format string \a format.
 *
 * @param[in] format The format string.
 *
 * @return The parsed format structure, or NULL on error.
 * The returned buffer must be freed with xbps_fmt_free().
 * @retval EINVAL Invalid format string.
 * @retval ERANGE Invalid alignment specifier.
 * @retval ENOMEM Memory allocation failure.
 */
struct xbps_fmt *xbps_fmt_parse(const char *format);

/**
 * @brief Releases memory associated with \a fmt.
 *
 * @param[in] fmt The format string.
 */
void xbps_fmt_free(struct xbps_fmt *fmt);

/**
 * @brief Print formatted text to \a fp.
 *
 * @param[in] fmt Format returned by struct xbps_fmt_parse().
 * @param[in] cb Callback function called for each variable in the format.
 * @param[in] data Userdata passed to the callback \a cb.
 * @param[in] fp File to print to.
 *
 * @return 0 on success or a negative errno.
 * @retval 0 Success
 */
int xbps_fmt(const struct xbps_fmt *fmt, xbps_fmt_cb *cb, void *data, FILE *fp);

/**
 * @brief Print formatted dictionary values to \a fp.
 *
 * Prints formatted dictionary values as specified by the parsed \a fmt
 * format string to \a fp.
 *
 * @param[in] fmt Format returned by struct xbps_fmt_parse().
 * @param[in] dict Dictionary to print values from.
 * @param[in] fp File to print to.
 *
 * @return 0 on success or value returned by \a cb.
 * @retval 0 Success
 */
int xbps_fmt_dictionary(const struct xbps_fmt *fmt, xbps_dictionary_t dict, FILE *fp);

/**
 * @brief Print formatted dictionary values to \a fp.
 *
 * Prints formatted dictionary values as specified by the format string
 * \a format to \a fp.
 *
 * @param[in] format Format string.
 * @param[in] dict Dictionary to print values from.
 * @param[in] fp File to print to.
 *
 * @return 0 on success or value returned by \a cb.
 * @retval 0 Success
 */
int xbps_fmts_dictionary(const char *format, xbps_dictionary_t dict, FILE *fp);

/**
 * @brief Print formatted dictionary to \a fp.
 *
 * Print the formatted dictionary according to the \a format format string
 * to \a fp.
 *
 * @param[in] format Format string.
 * @param[in] cb Callback function called for each variable in the format.
 * @param[in] data Userdata passed to the callback \a cb.
 * @param[in] fp File to print to.
 *
 * @return 0 on success.
 * @retval 0 Success.
 * @retval -EINVAL Invalid format string.
 * @retval -ERANGE Invalid alignment specifier.
 * @retval -ENOMEM Memory allocation failure.
 */
int xbps_fmts(const char *format, xbps_fmt_cb *cb, void *data, FILE *fp);

/**
 * @brief Print formatted number to \a fp.
 *
 * Prints the number \a num to \a fp according to the specification \a var.
 *
 * @param[in] var Variable to format.
 * @param[in] num Number to print.
 * @param[in] fp File to print to.
 *
 * @return Returns 0 on success.
 */
int xbps_fmt_print_number(const struct xbps_fmt_var *var, int64_t num, FILE *fp);

/**
 * @brief Print formatted string to \a fp.
 *
 * Prints the string \a str to \a fp according to the specification \a var.
 *
 * @param[in] var Variable to print.
 * @param[in] str String to print.
 * @param[in] len Length of the string or 0.
 * @param[in] fp File to print to.
 *
 * @return Returns 0 on success.
 */
int xbps_fmt_print_string(const struct xbps_fmt_var *var, const char *str, size_t len, FILE *fp);

/**
 * @brief Print formatted ::xbps_object_t to \a fp.
 *
 * Prints the ::xbps_object_t \a obj to \a fp according to the specification in \a var.
 *
 * @param[in] var Variable to format.
 * @param[in] obj The object to print.
 * @param[in] fp File to print to.
 *
 * @return Returns 0 on success.
 */
int xbps_fmt_print_object(const struct xbps_fmt_var *var, xbps_object_t obj, FILE *fp);

/**@}*/

#endif /* !_XBPS_FMT_H_ */
