/**
 * @mainpage The X Binary Package System Library API
 * @section intro_sec Introduction
 *
 * XBPS is a new binary package system designed and implemented from
 * scratch, by <b>Juan Romero Pardines</b>. This document describes
 * the API used by the XBPS Library, that is the base to implement
 * a package manager frontend, such as is implemented in the xbps
 * command line interfaces.
 *
 * XBPS uses extensively NetBSD's proplib, a library that provides an
 * abstract interface for creating and manipulating property lists.
 * Property lists have object types for boolean values, opaque data, numbers,
 * and strings. Structure is provided by the array and dictionary collection
 * types. Property lists can be passed across protection boundaries by
 * translating them to an external representation. This external representation
 * is an XML document whose format is described by the following DTD:
 *
 * http://www.apple.com/DTDs/PropertyList-1.0.dtd
 *
 * NetBSD's proplib has been choosed because it's fast, extensible, and easy
 * to use. These are the three facts I mentioned:
 *
 *  - <b>Fast</b> because proplib uses an ultra optimized
 *    <em>red-black tree</em> implementation to store and find all its objects,
 *    the same implementation has been used in commercial projects by
 *    <em>Apple Inc</em>.
 *
 *  - <b>Extensible</b> because you don't have to worry about ABI problems
 *    with its objects, arrays and dictionaries can be extended without such
 *    problems.
 *
 *  - <b>Easy</b> to use (and learn) because it has a superb documentation
 *    available in the form of manual pages.
 *
 * Not to mention that its arrays and dictionaries can be externalized to
 * files (known as plists) and <b>are always written atomically</b>. You
 * have the whole file or don't have it at all.
 */
