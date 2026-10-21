/*	$NetBSD: prop_object.c,v 1.42 2025/05/14 03:25:46 thorpej Exp $	*/

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
#include <prop/prop_object.h>

#ifdef _PROP_NEED_REFCNT_MTX
static pthread_mutex_t _prop_refcnt_mtx = PTHREAD_MUTEX_INITIALIZER;
#endif /* _PROP_NEED_REFCNT_MTX */

#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>

/*
 * _prop_object_init --
 *	Initialize an object.  Called when sub-classes create
 *	an instance.
 */
void
_prop_object_init(struct _prop_object *po, const struct _prop_object_type *pot)
{

	po->po_type = pot;
	po->po_refcnt = 1;
}

/*
 * _prop_object_fini --
 *	Finalize an object.  Called when sub-classes destroy
 *	an instance.
 */
/*ARGSUSED*/
void
_prop_object_fini(struct _prop_object *po _PROP_ARG_UNUSED)
{
	/* Nothing to do, currently. */
}

/*
 * prop_object_retain --
 *	Increment the reference count on an object.
 */
_PROP_EXPORT void
prop_object_retain(prop_object_t obj)
{
	struct _prop_object *po = obj;
	uint32_t ncnt _PROP_ARG_UNUSED;

	_PROP_ATOMIC_INC32_NV(&po->po_refcnt, ncnt);
	_PROP_ASSERT(ncnt != 0);
}

/*
 * prop_object_release_emergency
 *	A direct free with prop_object_release failed.
 *	Walk down the tree until a leaf is found and
 *	free that. Do not recurse to avoid stack overflows.
 *
 *	This is a slow edge condition, but necessary to
 *	guarantee that an object can always be freed.
 */
static void
prop_object_release_emergency(prop_object_t obj)
{
	struct _prop_object *po;
	void (*unlock)(void);
	prop_object_t parent = NULL;
	uint32_t ocnt;

	for (;;) {
		po = obj;
		_PROP_ASSERT(obj);

		if (po->po_type->pot_lock != NULL)
		po->po_type->pot_lock();

		/* Save pointerto unlock function */
		unlock = po->po_type->pot_unlock;

		/* Dance a bit to make sure we always get the non-racy ocnt */
		_PROP_ATOMIC_DEC32_NV(&po->po_refcnt, ocnt);
		ocnt++;
		_PROP_ASSERT(ocnt != 0);

		if (ocnt != 1) {
			if (unlock != NULL)
				unlock();
			break;
		}

		_PROP_ASSERT(po->po_type);
		if ((po->po_type->pot_free)(NULL, &obj) ==
		    _PROP_OBJECT_FREE_DONE) {
			if (unlock != NULL)
				unlock();
			break;
		}

		if (unlock != NULL)
			unlock();

		parent = po;
		_PROP_ATOMIC_INC32(&po->po_refcnt);
	}
	_PROP_ASSERT(parent);
	/* One object was just freed. */
	po = parent;
	(*po->po_type->pot_emergency_free)(parent);
}

/*
 * prop_object_release --
 *	Decrement the reference count on an object.
 *
 *	Free the object if we are releasing the final
 *	reference.
 */
_PROP_EXPORT void
prop_object_release(prop_object_t obj)
{
	struct _prop_object *po;
	struct _prop_stack stack;
	void (*unlock)(void);
	int ret;
	uint32_t ocnt;

	_prop_stack_init(&stack);

	do {
		do {
			po = obj;
			_PROP_ASSERT(obj);

			if (po->po_type->pot_lock != NULL)
				po->po_type->pot_lock();

			/* Save pointer to object unlock function */
			unlock = po->po_type->pot_unlock;

			_PROP_ATOMIC_DEC32_NV(&po->po_refcnt, ocnt);
			ocnt++;
			_PROP_ASSERT(ocnt != 0);

			if (ocnt != 1) {
				ret = 0;
				if (unlock != NULL)
					unlock();
				break;
			}

			ret = (po->po_type->pot_free)(&stack, &obj);

			if (unlock != NULL)
				unlock();

			if (ret == _PROP_OBJECT_FREE_DONE)
				break;

			_PROP_ATOMIC_INC32(&po->po_refcnt);
		} while (ret == _PROP_OBJECT_FREE_RECURSE);
		if (ret == _PROP_OBJECT_FREE_FAILED)
			prop_object_release_emergency(obj);
	} while (_prop_stack_pop(&stack, &obj, NULL, NULL, NULL));
}

/*
 * prop_object_type --
 *	Return the type of an object.
 */
_PROP_EXPORT prop_type_t
prop_object_type(prop_object_t obj)
{
	struct _prop_object *po = obj;

	if (obj == NULL)
		return (PROP_TYPE_UNKNOWN);

	return (po->po_type->pot_type);
}

/*
 * prop_object_equals --
 *	Returns true if thw two objects are equivalent.
 */
_PROP_EXPORT bool
prop_object_equals(prop_object_t obj1, prop_object_t obj2)
{
	return (prop_object_equals_with_error(obj1, obj2, NULL));
}

_PROP_EXPORT bool
prop_object_equals_with_error(prop_object_t obj1, prop_object_t obj2,
    bool *error_flag)
{
	struct _prop_object *po1;
	struct _prop_object *po2;
	void *stored_pointer1, *stored_pointer2;
	prop_object_t next_obj1, next_obj2;
	struct _prop_stack stack;
	_prop_object_equals_rv_t ret;

	_prop_stack_init(&stack);
	if (error_flag)
		*error_flag = false;

 start_subtree:
	stored_pointer1 = NULL;
	stored_pointer2 = NULL;
	po1 = obj1;
	po2 = obj2;

	if (po1->po_type != po2->po_type)
		return (false);

 continue_subtree:
	ret = (*po1->po_type->pot_equals)(obj1, obj2,
					  &stored_pointer1, &stored_pointer2,
					  &next_obj1, &next_obj2);
	if (ret == _PROP_OBJECT_EQUALS_FALSE)
		goto finish;
	if (ret == _PROP_OBJECT_EQUALS_TRUE) {
		if (!_prop_stack_pop(&stack, &obj1, &obj2,
				     &stored_pointer1, &stored_pointer2))
			return true;
		po1 = obj1;
		po2 = obj2;
		goto continue_subtree;
	}
	_PROP_ASSERT(ret == _PROP_OBJECT_EQUALS_RECURSE);

	if (!_prop_stack_push(&stack, obj1, obj2,
			      stored_pointer1, stored_pointer2)) {
		if (error_flag)
			*error_flag = true;
		goto finish;
	}
	obj1 = next_obj1;
	obj2 = next_obj2;
	goto start_subtree;

finish:
	while (_prop_stack_pop(&stack, &obj1, &obj2, NULL, NULL)) {
		po1 = obj1;
		(*po1->po_type->pot_equals_finish)(obj1, obj2);
	}
	return (false);
}

/*
 * prop_object_iterator_next --
 *	Return the next item during an iteration.
 */
_PROP_EXPORT prop_object_t
prop_object_iterator_next(prop_object_iterator_t pi)
{

	return ((*pi->pi_next_object)(pi));
}

/*
 * prop_object_iterator_reset --
 *	Reset the iterator to the first object so as to restart
 *	iteration.
 */
_PROP_EXPORT void
prop_object_iterator_reset(prop_object_iterator_t pi)
{

	(*pi->pi_reset)(pi);
}

/*
 * prop_object_iterator_release --
 *	Release the object iterator.
 */
_PROP_EXPORT void
prop_object_iterator_release(prop_object_iterator_t pi)
{

	prop_object_release(pi->pi_obj);
	_PROP_FREE(pi, M_TEMP);
}
