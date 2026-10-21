/*	$NetBSD: prop_array.c,v 1.27 2025/05/14 03:25:45 thorpej Exp $	*/

/*-
 * Copyright (c) 2006, 2007, 2025 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "prop_object_impl.h"
#include <prop/prop_array.h>

#include <errno.h>

struct _prop_array {
	struct _prop_object	pa_obj;
	_PROP_RWLOCK_DECL(pa_rwlock)
	prop_object_t *		pa_array;
	unsigned int		pa_capacity;
	unsigned int		pa_count;
	int			pa_flags;

	uint32_t		pa_version;
};

#define PA_F_IMMUTABLE		0x01	/* array is immutable */

_PROP_POOL_INIT(_prop_array_pool, sizeof(struct _prop_array), "proparay")
_PROP_MALLOC_DEFINE(M_PROP_ARRAY, "prop array",
		    "property array container object")

static const struct _prop_object_type_tags _prop_array_type_tags = {
	.xml_tag		=	"array",
	.json_open_tag		=	"[",
	.json_close_tag		=	"]",
	.json_empty_sep		=	" ",
};

struct _prop_array_iterator {
	struct _prop_object_iterator pai_base;
	unsigned int		pai_index;
};

static _prop_object_free_rv_t
		_prop_array_free(prop_stack_t, prop_object_t *);
static void	_prop_array_emergency_free(prop_object_t);
static bool	_prop_array_externalize(
				struct _prop_object_externalize_context *,
				void *);
static _prop_object_equals_rv_t
		_prop_array_equals(prop_object_t, prop_object_t,
				   void **, void **,
				   prop_object_t *, prop_object_t *);
static void	_prop_array_equals_finish(prop_object_t, prop_object_t);
static struct _prop_array_iterator *
		_prop_array_iterator_locked(prop_array_t);
static prop_object_t
		_prop_array_iterator_next_object_locked(void *);
static void	_prop_array_iterator_reset_locked(void *);

static const struct _prop_object_type _prop_object_type_array = {
	.pot_type		=	PROP_TYPE_ARRAY,
	.pot_free		=	_prop_array_free,
	.pot_emergency_free	=	_prop_array_emergency_free,
	.pot_extern		=	_prop_array_externalize,
	.pot_equals		=	_prop_array_equals,
	.pot_equals_finish	=	_prop_array_equals_finish,
};

#define prop_object_is_array(x)		\
	((x) != NULL && (x)->pa_obj.po_type == &_prop_object_type_array)

#define prop_array_is_immutable(x) (((x)->pa_flags & PA_F_IMMUTABLE) != 0)

#define EXPAND_STEP		16

static _prop_object_free_rv_t
_prop_array_free(prop_stack_t stack, prop_object_t *obj)
{
	prop_array_t pa = *obj;
	prop_object_t po;

	_PROP_ASSERT(pa->pa_count <= pa->pa_capacity);
	_PROP_ASSERT((pa->pa_capacity == 0 && pa->pa_array == NULL) ||
		     (pa->pa_capacity != 0 && pa->pa_array != NULL));

	/* The easy case is an empty array, just free and return. */
	if (pa->pa_count == 0) {
		if (pa->pa_array != NULL)
			_PROP_FREE(pa->pa_array, M_PROP_ARRAY);

		_PROP_RWLOCK_DESTROY(pa->pa_rwlock);

		_PROP_POOL_PUT(_prop_array_pool, pa);

		return (_PROP_OBJECT_FREE_DONE);
	}

	po = pa->pa_array[pa->pa_count - 1];
	_PROP_ASSERT(po != NULL);

	if (stack == NULL) {
		/*
		 * If we are in emergency release mode,
		 * just let caller recurse down.
		 */
		*obj = po;
		return (_PROP_OBJECT_FREE_FAILED);
	}

	/* Otherwise, try to push the current object on the stack. */
	if (!_prop_stack_push(stack, pa, NULL, NULL, NULL)) {
		/* Push failed, entering emergency release mode. */
		return (_PROP_OBJECT_FREE_FAILED);
	}
	/* Object pushed on stack, caller will release it. */
	--pa->pa_count;
	*obj = po;
	return (_PROP_OBJECT_FREE_RECURSE);
}

static void
_prop_array_emergency_free(prop_object_t obj)
{
	prop_array_t pa = obj;

	_PROP_ASSERT(pa->pa_count != 0);
	--pa->pa_count;
}

static bool
_prop_array_externalize(struct _prop_object_externalize_context *ctx,
			void *v)
{
	prop_array_t pa = v;
	struct _prop_object *po;
	struct _prop_array_iterator *pai;
	bool rv = false;
	const char * const sep =
	    ctx->poec_format == PROP_FORMAT_JSON ? "," : NULL;

	_PROP_ASSERT(ctx->poec_format == PROP_FORMAT_XML ||
		     ctx->poec_format == PROP_FORMAT_JSON);

	_PROP_RWLOCK_RDLOCK(pa->pa_rwlock);

	if (pa->pa_count == 0) {
		_PROP_RWLOCK_UNLOCK(pa->pa_rwlock);
		return (_prop_extern_append_empty_tag(ctx,
		    &_prop_array_type_tags));
	}

	if (_prop_extern_append_start_tag(ctx,
				&_prop_array_type_tags, NULL) == false ||
	    _prop_extern_end_line(ctx, NULL) == false)
		goto out;

	pai = _prop_array_iterator_locked(pa);
	if (pai == NULL)
		goto out;

	ctx->poec_depth++;
	_PROP_ASSERT(ctx->poec_depth != 0);

	while ((po = _prop_array_iterator_next_object_locked(pai)) != NULL) {
		if (_prop_extern_start_line(ctx) == false ||
		    (*po->po_type->pot_extern)(ctx, po) == false ||
		    _prop_extern_end_line(ctx,
				pai->pai_index < pa->pa_count ?
							sep : NULL) == false) {
			prop_object_iterator_release(&pai->pai_base);
			goto out;
		}
	}

	prop_object_iterator_release(&pai->pai_base);

	ctx->poec_depth--;
	if (_prop_extern_start_line(ctx) == false ||
	    _prop_extern_append_end_tag(ctx,
					&_prop_array_type_tags) == false) {
		goto out;
	}

	rv = true;

 out:
	_PROP_RWLOCK_UNLOCK(pa->pa_rwlock);
	return (rv);
}

/* ARGSUSED */
static _prop_object_equals_rv_t
_prop_array_equals(prop_object_t v1, prop_object_t v2,
    void **stored_pointer1, void **stored_pointer2,
    prop_object_t *next_obj1, prop_object_t *next_obj2)
{
	prop_array_t array1 = v1;
	prop_array_t array2 = v2;
	uintptr_t idx;
	_prop_object_equals_rv_t rv = _PROP_OBJECT_EQUALS_FALSE;

	if (array1 == array2)
		return (_PROP_OBJECT_EQUALS_TRUE);

	_PROP_ASSERT(*stored_pointer1 == *stored_pointer2);
	idx = (uintptr_t)*stored_pointer1;

	/* For the first iteration, lock the objects. */
	if (idx == 0) {
		if ((uintptr_t)array1 < (uintptr_t)array2) {
			_PROP_RWLOCK_RDLOCK(array1->pa_rwlock);
			_PROP_RWLOCK_RDLOCK(array2->pa_rwlock);
		} else {
			_PROP_RWLOCK_RDLOCK(array2->pa_rwlock);
			_PROP_RWLOCK_RDLOCK(array1->pa_rwlock);
		}
	}

	if (array1->pa_count != array2->pa_count)
		goto out;
	if (idx == array1->pa_count) {
		rv = _PROP_OBJECT_EQUALS_TRUE;
		goto out;
	}
	_PROP_ASSERT(idx < array1->pa_count);

	*stored_pointer1 = (void *)(idx + 1);
	*stored_pointer2 = (void *)(idx + 1);

	*next_obj1 = array1->pa_array[idx];
	*next_obj2 = array2->pa_array[idx];

	return (_PROP_OBJECT_EQUALS_RECURSE);

 out:
	_PROP_RWLOCK_UNLOCK(array1->pa_rwlock);
	_PROP_RWLOCK_UNLOCK(array2->pa_rwlock);
	return (rv);
}

static void
_prop_array_equals_finish(prop_object_t v1, prop_object_t v2)
{
	_PROP_RWLOCK_UNLOCK(((prop_array_t)v1)->pa_rwlock);
	_PROP_RWLOCK_UNLOCK(((prop_array_t)v2)->pa_rwlock);
}

static prop_array_t
_prop_array_alloc(unsigned int capacity)
{
	prop_array_t pa;
	prop_object_t *array;

	if (capacity != 0) {
		array = _PROP_CALLOC(capacity * sizeof(prop_object_t),
				     M_PROP_ARRAY);
		if (array == NULL)
			return (NULL);
	} else
		array = NULL;

	pa = _PROP_POOL_GET(_prop_array_pool);
	if (pa != NULL) {
		_prop_object_init(&pa->pa_obj, &_prop_object_type_array);
		pa->pa_obj.po_type = &_prop_object_type_array;

		_PROP_RWLOCK_INIT(pa->pa_rwlock);
		pa->pa_array = array;
		pa->pa_capacity = capacity;
		pa->pa_count = 0;
		pa->pa_flags = 0;

		pa->pa_version = 0;
	} else if (array != NULL)
		_PROP_FREE(array, M_PROP_ARRAY);

	return (pa);
}

static bool
_prop_array_expand(prop_array_t pa, unsigned int capacity)
{
	prop_object_t *array, *oarray;

	/*
	 * Array must be WRITE-LOCKED.
	 */

	oarray = pa->pa_array;

	array = _PROP_CALLOC(capacity * sizeof(*array), M_PROP_ARRAY);
	if (array == NULL)
		return (false);
	if (oarray != NULL)
		memcpy(array, oarray, pa->pa_capacity * sizeof(*array));
	pa->pa_array = array;
	pa->pa_capacity = capacity;

	if (oarray != NULL)
		_PROP_FREE(oarray, M_PROP_ARRAY);

	return (true);
}

static prop_object_t
_prop_array_iterator_next_object_locked(void *v)
{
	struct _prop_array_iterator *pai = v;
	prop_array_t pa = pai->pai_base.pi_obj;
	prop_object_t po = NULL;

	_PROP_ASSERT(prop_object_is_array(pa));

	if (pa->pa_version != pai->pai_base.pi_version)
		goto out;	/* array changed during iteration */

	_PROP_ASSERT(pai->pai_index <= pa->pa_count);

	if (pai->pai_index == pa->pa_count)
		goto out;	/* we've iterated all objects */

	po = pa->pa_array[pai->pai_index];
	pai->pai_index++;

 out:
	return (po);
}

static prop_object_t
_prop_array_iterator_next_object(void *v)
{
	struct _prop_array_iterator *pai = v;
	prop_array_t pa _PROP_ARG_UNUSED = pai->pai_base.pi_obj;
	prop_object_t po;

	_PROP_ASSERT(prop_object_is_array(pa));

	_PROP_RWLOCK_RDLOCK(pa->pa_rwlock);
	po = _prop_array_iterator_next_object_locked(pai);
	_PROP_RWLOCK_UNLOCK(pa->pa_rwlock);
	return (po);
}

static void
_prop_array_iterator_reset_locked(void *v)
{
	struct _prop_array_iterator *pai = v;
	prop_array_t pa = pai->pai_base.pi_obj;

	_PROP_ASSERT(prop_object_is_array(pa));

	pai->pai_index = 0;
	pai->pai_base.pi_version = pa->pa_version;
}

static void
_prop_array_iterator_reset(void *v)
{
	struct _prop_array_iterator *pai = v;
	prop_array_t pa _PROP_ARG_UNUSED = pai->pai_base.pi_obj;

	_PROP_ASSERT(prop_object_is_array(pa));

	_PROP_RWLOCK_RDLOCK(pa->pa_rwlock);
	_prop_array_iterator_reset_locked(pai);
	_PROP_RWLOCK_UNLOCK(pa->pa_rwlock);
}

/*
 * prop_array_create --
 *	Create an empty array.
 */
_PROP_EXPORT prop_array_t
prop_array_create(void)
{

	return (_prop_array_alloc(0));
}

/*
 * prop_array_create_with_capacity --
 *	Create an array with the capacity to store N objects.
 */
_PROP_EXPORT prop_array_t
prop_array_create_with_capacity(unsigned int capacity)
{

	return (_prop_array_alloc(capacity));
}

/*
 * prop_array_copy --
 *	Copy an array.	The new array has an initial capacity equal to
 *	the number of objects stored in the original array.  The new
 *	array contains references to the original array's objects, not
 *	copies of those objects (i.e. a shallow copy).
 */
_PROP_EXPORT prop_array_t
prop_array_copy(prop_array_t opa)
{
	prop_array_t pa;
	prop_object_t po;
	unsigned int idx;

	if (! prop_object_is_array(opa))
		return (NULL);

	_PROP_RWLOCK_RDLOCK(opa->pa_rwlock);

	pa = _prop_array_alloc(opa->pa_count);
	if (pa != NULL) {
		for (idx = 0; idx < opa->pa_count; idx++) {
			po = opa->pa_array[idx];
			prop_object_retain(po);
			pa->pa_array[idx] = po;
		}
		pa->pa_count = opa->pa_count;
		pa->pa_flags = opa->pa_flags;
	}
	_PROP_RWLOCK_UNLOCK(opa->pa_rwlock);
	return (pa);
}

/*
 * prop_array_copy_mutable --
 *	Like prop_array_copy(), but the resulting array is mutable.
 */
_PROP_EXPORT prop_array_t
prop_array_copy_mutable(prop_array_t opa)
{
	prop_array_t pa;

	pa = prop_array_copy(opa);
	if (pa != NULL)
		pa->pa_flags &= ~PA_F_IMMUTABLE;

	return (pa);
}

/*
 * prop_array_capacity --
 *	Return the capacity of the array.
 */
_PROP_EXPORT unsigned int
prop_array_capacity(prop_array_t pa)
{
	unsigned int rv;

	if (! prop_object_is_array(pa))
		return (0);

	_PROP_RWLOCK_RDLOCK(pa->pa_rwlock);
	rv = pa->pa_capacity;
	_PROP_RWLOCK_UNLOCK(pa->pa_rwlock);

	return (rv);
}

/*
 * prop_array_count --
 *	Return the number of objects stored in the array.
 */
_PROP_EXPORT unsigned int
prop_array_count(prop_array_t pa)
{
	unsigned int rv;

	if (! prop_object_is_array(pa))
		return (0);

	_PROP_RWLOCK_RDLOCK(pa->pa_rwlock);
	rv = pa->pa_count;
	_PROP_RWLOCK_UNLOCK(pa->pa_rwlock);

	return (rv);
}

/*
 * prop_array_ensure_capacity --
 *	Ensure that the array has the capacity to store the specified
 *	total number of objects (including the objects already stored
 *	in the array).
 */
_PROP_EXPORT bool
prop_array_ensure_capacity(prop_array_t pa, unsigned int capacity)
{
	bool rv;

	if (! prop_object_is_array(pa))
		return (false);

	_PROP_RWLOCK_WRLOCK(pa->pa_rwlock);
	if (capacity > pa->pa_capacity)
		rv = _prop_array_expand(pa, capacity);
	else
		rv = true;
	_PROP_RWLOCK_UNLOCK(pa->pa_rwlock);

	return (rv);
}

static struct _prop_array_iterator *
_prop_array_iterator_locked(prop_array_t pa)
{
	struct _prop_array_iterator *pai;

	if (! prop_object_is_array(pa))
		return (NULL);

	pai = _PROP_CALLOC(sizeof(*pai), M_TEMP);
	if (pai == NULL)
		return (NULL);
	pai->pai_base.pi_next_object = _prop_array_iterator_next_object;
	pai->pai_base.pi_reset = _prop_array_iterator_reset;
	prop_object_retain(pa);
	pai->pai_base.pi_obj = pa;
	_prop_array_iterator_reset_locked(pai);

	return pai;
}

/*
 * prop_array_iterator --
 *	Return an iterator for the array.  The array is retained by
 *	the iterator.
 */
_PROP_EXPORT prop_object_iterator_t
prop_array_iterator(prop_array_t pa)
{
	struct _prop_array_iterator *pai;

	_PROP_RWLOCK_RDLOCK(pa->pa_rwlock);
	pai = _prop_array_iterator_locked(pa);
	_PROP_RWLOCK_UNLOCK(pa->pa_rwlock);
	return &pai->pai_base;
}

/*
 * prop_array_make_immutable --
 *	Make the array immutable.
 */
_PROP_EXPORT void
prop_array_make_immutable(prop_array_t pa)
{

	_PROP_RWLOCK_WRLOCK(pa->pa_rwlock);
	if (prop_array_is_immutable(pa) == false)
		pa->pa_flags |= PA_F_IMMUTABLE;
	_PROP_RWLOCK_UNLOCK(pa->pa_rwlock);
}

/*
 * prop_array_mutable --
 *	Returns true if the array is mutable.
 */
_PROP_EXPORT bool
prop_array_mutable(prop_array_t pa)
{
	bool rv;

	_PROP_RWLOCK_RDLOCK(pa->pa_rwlock);
	rv = prop_array_is_immutable(pa) == false;
	_PROP_RWLOCK_UNLOCK(pa->pa_rwlock);

	return (rv);
}

/*
 * prop_array_get --
 *	Return the object stored at the specified array index.
 */
_PROP_EXPORT prop_object_t
prop_array_get(prop_array_t pa, unsigned int idx)
{
	prop_object_t po = NULL;

	if (! prop_object_is_array(pa))
		return (NULL);

	_PROP_RWLOCK_RDLOCK(pa->pa_rwlock);
	if (idx >= pa->pa_count)
		goto out;
	po = pa->pa_array[idx];
	_PROP_ASSERT(po != NULL);
 out:
	_PROP_RWLOCK_UNLOCK(pa->pa_rwlock);
	return (po);
}

static bool
_prop_array_add(prop_array_t pa, prop_object_t po)
{

	/*
	 * Array must be WRITE-LOCKED.
	 */

	_PROP_ASSERT(pa->pa_count <= pa->pa_capacity);

	if (prop_array_is_immutable(pa) ||
	    (pa->pa_count == pa->pa_capacity &&
	    _prop_array_expand(pa, pa->pa_capacity + EXPAND_STEP) == false))
		return (false);

	prop_object_retain(po);
	pa->pa_array[pa->pa_count++] = po;
	pa->pa_version++;

	return (true);
}

static bool
_prop_array_add_first(prop_array_t pa, prop_object_t po)
{
	unsigned int cnt;
	/*
	 * Array must be WRITE-LOCKED.
	 */

	_PROP_ASSERT(pa->pa_count <= pa->pa_capacity);

	if (prop_array_is_immutable(pa) ||
	    (pa->pa_count == pa->pa_capacity &&
	    _prop_array_expand(pa, pa->pa_capacity + EXPAND_STEP) == false))
		return false;

	prop_object_retain(po);
	if (pa->pa_count) {
		cnt = pa->pa_count+1;
		/* move all stored elements to the right */
		while (--cnt) {
			prop_object_t opo = pa->pa_array[cnt-1];
			_PROP_ASSERT(opo != NULL);
			prop_object_retain(opo);
			pa->pa_array[cnt] = opo;
			prop_object_release(opo);
		}
		/* passed in object is now the first element */
		pa->pa_array[0] = po;
		pa->pa_version++;
		pa->pa_count++;
	} else {
		pa->pa_array[pa->pa_count++] = po;
		pa->pa_version++;
	}
	return true;
}

/*
 * prop_array_set --
 *	Store a reference to an object at the specified array index.
 *	This method is not allowed to create holes in the array; the
 *	caller must either be setting the object just beyond the existing
 *	count or replacing an already existing object reference.
 */
_PROP_EXPORT bool
prop_array_set(prop_array_t pa, unsigned int idx, prop_object_t po)
{
	prop_object_t opo;
	bool rv = false;

	if (! prop_object_is_array(pa))
		return (false);

	_PROP_RWLOCK_WRLOCK(pa->pa_rwlock);

	if (prop_array_is_immutable(pa))
		goto out;

	if (idx == pa->pa_count) {
		rv = _prop_array_add(pa, po);
		goto out;
	}

	_PROP_ASSERT(idx < pa->pa_count);

	opo = pa->pa_array[idx];
	_PROP_ASSERT(opo != NULL);

	prop_object_retain(po);
	pa->pa_array[idx] = po;
	pa->pa_version++;

	prop_object_release(opo);

	rv = true;

 out:
	_PROP_RWLOCK_UNLOCK(pa->pa_rwlock);
	return (rv);
}

/*
 * prop_array_add --
 *	Add a reference to an object to the specified array, appending
 *	to the end and growing the array's capacity, if necessary.
 */
_PROP_EXPORT bool
prop_array_add(prop_array_t pa, prop_object_t po)
{
	bool rv;

	if (! prop_object_is_array(pa))
		return (false);

	_PROP_RWLOCK_WRLOCK(pa->pa_rwlock);
	rv = _prop_array_add(pa, po);
	_PROP_RWLOCK_UNLOCK(pa->pa_rwlock);

	return (rv);
}

/*
 * prop_array_add_first --
 *	Add a reference to an object to the specified array, inserting it
 *	as first element, moving all objects to the right and growing the
 *	array's capacity, if necessary.
 */
bool
prop_array_add_first(prop_array_t pa, prop_object_t po)
{
	bool rv;

	if (! prop_object_is_array(pa))
		return (false);

	_PROP_RWLOCK_WRLOCK(pa->pa_rwlock);
	rv = _prop_array_add_first(pa, po);
	_PROP_RWLOCK_UNLOCK(pa->pa_rwlock);

	return (rv);
}
/*
 * prop_array_remove --
 *	Remove the reference to an object from an array at the specified
 *	index.	The array will be compacted following the removal.
 */
_PROP_EXPORT void
prop_array_remove(prop_array_t pa, unsigned int idx)
{
	prop_object_t po;

	if (! prop_object_is_array(pa))
		return;

	_PROP_RWLOCK_WRLOCK(pa->pa_rwlock);

	_PROP_ASSERT(idx < pa->pa_count);

	/* XXX Should this be a _PROP_ASSERT()? */
	if (prop_array_is_immutable(pa)) {
		_PROP_RWLOCK_UNLOCK(pa->pa_rwlock);
		return;
	}

	po = pa->pa_array[idx];
	_PROP_ASSERT(po != NULL);

	for (++idx; idx < pa->pa_count; idx++)
		pa->pa_array[idx - 1] = pa->pa_array[idx];
	pa->pa_count--;
	pa->pa_version++;

	_PROP_RWLOCK_UNLOCK(pa->pa_rwlock);

	prop_object_release(po);
}

/*
 * prop_array_equals --
 *	Return true if the two arrays are equivalent.  Note we do a
 *	by-value comparison of the objects in the array.
 */
_PROP_EXPORT bool
prop_array_equals(prop_array_t array1, prop_array_t array2)
{
	if (!prop_object_is_array(array1) || !prop_object_is_array(array2))
		return (false);

	return (prop_object_equals(array1, array2));
}

/*
 * prop_array_externalize --
 *	Externalize an array in XML format.
 */
_PROP_EXPORT char *
prop_array_externalize(prop_array_t pa)
{
	return _prop_object_externalize(&pa->pa_obj, PROP_FORMAT_XML);
}

/*
 * _prop_array_internalize --
 *	Parse an <array>...</array> and return the object created from the
 *	external representation.
 */
static bool _prop_array_internalize_body(prop_stack_t, prop_object_t *,
    struct _prop_object_internalize_context *);

bool
_prop_array_internalize(prop_stack_t stack, prop_object_t *obj,
    struct _prop_object_internalize_context *ctx)
{
	/* We don't currently understand any attributes. */
	if (ctx->poic_tagattr != NULL)
		return (true);

	*obj = prop_array_create();
	/*
	 * We are done if the create failed or no child elements exist.
	 */
	if (*obj == NULL || ctx->poic_is_empty_element)
		return (true);

	/*
	 * Opening tag is found, now continue to the first element.
	 */
	return (_prop_array_internalize_body(stack, obj, ctx));
}

static bool
_prop_array_internalize_continue(prop_stack_t stack,
    prop_object_t *obj,
    struct _prop_object_internalize_context *ctx,
    void *data, prop_object_t child)
{
	prop_array_t array;

	_PROP_ASSERT(data == NULL);

	if (child == NULL)
		goto bad; /* Element could not be parsed. */

	array = *obj;

	if (prop_array_add(array, child) == false) {
		prop_object_release(child);
		goto bad;
	}
	prop_object_release(child);

	/*
	 * Current element is processed and added, look for next.
	 * For JSON, we'll skip the comma separator, if present.
	 *
	 * By doing this here, we correctly error out if a separator
	 * is found other than after an element, but this does mean
	 * that we do allow a trailing comma after the final element
	 * which isn't allowed in the JSON spec, but seems pretty
	 * harmless (and there are other JSON parsers that also allow
	 * it).
	 *
	 * Conversely, we don't want to *require* the separator if the
	 * spec doesn't require it, and we don't know what's next in
	 * the buffer, so we basically treat the separator as completely
	 * optional.  Since there does not appear to be any ambiguity,
	 * this also seems pretty harmless.
	 *
	 * (FWIW, RFC 8259 section 9 seems to specifically allow this.)
	 */
	if (ctx->poic_format == PROP_FORMAT_JSON) {
		ctx->poic_cp = _prop_intern_skip_whitespace(ctx->poic_cp);
		if (*ctx->poic_cp == ',') {
			ctx->poic_cp++;
		}
	}
	return (_prop_array_internalize_body(stack, obj, ctx));

 bad:
	prop_object_release(*obj);
	*obj = NULL;
	return (true);
}

static bool
_prop_array_internalize_body(prop_stack_t stack, prop_object_t *obj,
    struct _prop_object_internalize_context *ctx)
{
	prop_array_t array = *obj;

	_PROP_ASSERT(array != NULL);

	if (ctx->poic_format == PROP_FORMAT_JSON) {
		ctx->poic_cp = _prop_intern_skip_whitespace(ctx->poic_cp);

		/* Check to see if this is the end of the array. */
		if (*ctx->poic_cp == ']') {
			/* It is, so don't iterate any further. */
			ctx->poic_cp++;
			return true;
		}
	} else {
		/* Fetch the next tag. */
		if (_prop_xml_intern_find_tag(ctx, NULL,
					_PROP_TAG_TYPE_EITHER) == false)
			goto bad;

		/* Check to see if this is the end of the array. */
		if (_PROP_TAG_MATCH(ctx, "array") &&
		    ctx->poic_tag_type == _PROP_TAG_TYPE_END) {
			/* It is, so don't iterate any further. */
			return (true);
		}
	}

	if (_prop_stack_push(stack, array,
			     _prop_array_internalize_continue, NULL, NULL))
		return (false);

 bad:
	prop_object_release(array);
	*obj = NULL;
	return (true);
}

/*
 * prop_array_internalize --
 *	Create an array by parsing the external representation.
 */
_PROP_EXPORT prop_array_t
prop_array_internalize(const char *data)
{
	return _prop_object_internalize(data, &_prop_array_type_tags);
}

/*
 * prop_array_externalize_to_file --
 *	Externalize an array to the specified file.
 */
_PROP_EXPORT bool
prop_array_externalize_to_file(prop_array_t array, const char *fname)
{
	return _prop_object_externalize_to_file(&array->pa_obj, fname,
	    PROP_FORMAT_XML);
}

/*
 * prop_array_internalize_from_file --
 *	Internalize an array from a file.
 */
_PROP_EXPORT prop_array_t
prop_array_internalize_from_file(const char *fname)
{
	return _prop_object_internalize_from_file(fname,
	    &_prop_array_type_tags);
}
