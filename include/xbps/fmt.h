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
 * @struct xbps_fmt xbps.h "xbps.h"
 * @brief Structure of parsed format string variable.
 */
struct xbps_fmt {
	/**
	 * @private
	 * @var prefix
	 * @brief Prefix of the format chunk.
	 */
	char *prefix;
	/**
	 * @var var
	 * @brief Variable name.
	 */
	char *var;
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
 * @struct xbps_fmt_def xbps.h "xbps.h"
 * @brief Structure of parsed format specifier.
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
 * @struct xbps_fmt_spec xbps.h "xbps.h"
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
 * A callback function should write data associated with \a var to \a fp and use
 * \a w as alignment specifier.
 *
 * @param[in] fp The file to print to.
 * @param[in] fmt The format specifier.
 * @param[in] data Userdata passed to the xbps_fmt() function.
 */
typedef int (xbps_fmt_cb)(FILE *fp, const struct xbps_fmt *fmt, void *data);

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
 * Prints the number \a num to \a fp according to the specification \a spec.
 *
 * @param[in] fmt Format specification.
 * @param[in] num Number to print.
 * @param[in] fp File to print to.
 *
 * @return Returns 0 on success.
 */
int xbps_fmt_print_number(const struct xbps_fmt *fmt, int64_t num, FILE *fp);

/**
 * @brief Print formatted string to \a fp.
 *
 * Prints the string \a str to \a fp according to the specification \a spec.
 *
 * @param[in] fmt Format specification.
 * @param[in] str String to print.
 * @param[in] len Length of the string or 0.
 * @param[in] fp File to print to.
 *
 * @return Returns 0 on success.
 */
int xbps_fmt_print_string(const struct xbps_fmt *fmt, const char *str, size_t len, FILE *fp);

/**
 * @brief Print formatted ::xbps_object_t to \a fp.
 *
 * Prints the ::xbps_object_t \a obj to \a fp according to the specification \a spec.
 *
 * @param[in] spec Format specification.
 * @param[in] obj The object to print.
 * @param[in] fp File to print to.
 *
 * @return Returns 0 on success.
 */
int xbps_fmt_print_object(const struct xbps_fmt *fmt, xbps_object_t obj, FILE *fp);

/**@}*/

#endif /* !_XBPS_FMT_H_ */
