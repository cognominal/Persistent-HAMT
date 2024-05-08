#include "moar.h"
#include "hamt-node.h"
#include "limits.h"

/* This representation's function pointer table. */
static const MVMREPROps VMArray_this_repr;

MVM_STATIC_INLINE void enter_single_user(MVMThreadContext *tc,
                                         MVMHamtNodeBody *arr)
{
#if MVM_ARRAY_CONC_DEBUG
    if (!MVM_trycas(&(arr->in_use), 0, 1)) {
        MVM_dump_backtrace(tc);
        MVM_exception_throw_adhoc(tc, "Array may not be used concurrently");
    }
#endif
}
static void exit_single_user(MVMThreadContext *tc, MVMHamtNodeBody *arr)
{
#if MVM_ARRAY_CONC_DEBUG
    arr->in_use = 0;
#endif
}

#define MVM_MAX(a, b) ((a) > (b) ? (a) : (b))
#define MVM_MIN(a, b) ((a) < (b) ? (a) : (b))

/* Creates a new type object of this representation, and associates it with
 * the given HOW. */
static MVMObject *type_object_for(MVMThreadContext *tc, MVMObject *HOW)
{
    MVMSTable *st = MVM_gc_allocate_stable(tc, &VMArray_this_repr, HOW);

    MVMROOT(tc, st, {
        MVMObject *obj = MVM_gc_allocate_type_object(tc, st);
        MVMHamtNodeREPRData *repr_data =
            (MVMHamtNodeREPRData *)MVM_malloc(sizeof(MVMHamtNodeREPRData));

        // repr_data->slot_type = MVM_ARRAY_OBJ;
        // repr_data->elem_size = sizeof(MVMObject *);
        // repr_data->elem_type = NULL;

        // MVM_ASSIGN_REF(tc, &(st->header), st->WHAT, obj);
        // st->size = sizeof(MVMHamtNode);
        // st->REPR_data = repr_data;
    });

    return st->WHAT;
}

/* Copies the body of one object to another. The result has the space
 * needed for the current number of elements, which may not be the
 * entire allocated slot size. */
static void copy_to(MVMThreadContext *tc, MVMSTable *st, void *src,
                    MVMObject *dest_root, void *dest)
{
    // MVMHamtNodeREPRData *repr_data = (MVMHamtNodeREPRData *)st->REPR_data;
    // MVMHamtNodeBody *src_body = (MVMHamtNodeBody *)src;
    // MVMHamtNodeBody *dest_body = (MVMHamtNodeBody *)dest;
    // dest_body->elems = src_body->elems;
    // dest_body->ssize = src_body->elems;
    // dest_body->start = 0;
    // if (dest_body->elems > 0) {
    //     size_t mem_size = dest_body->ssize * repr_data->elem_size;
    //     size_t start_pos = src_body->start * repr_data->elem_size;
    //     char *copy_start = ((char *)src_body->slots.any) + start_pos;
    //     dest_body->slots.any = MVM_malloc(mem_size);
    //     memcpy(dest_body->slots.any, copy_start, mem_size);
    // } else {
    //     dest_body->slots.any = NULL;
    // }
}

/* Adds held objects to the GC worklist. */
static void VMArray_gc_mark(MVMThreadContext *tc, MVMSTable *st, void *data,
                            MVMGCWorklist *worklist)
{
}

/* Called by the VM in order to free memory associated with this object. */
static void gc_free(MVMThreadContext *tc, MVMObject *obj)
{
}

/* Marks the representation data in an STable.*/
static void gc_mark_repr_data(MVMThreadContext *tc, MVMSTable *st,
                              MVMGCWorklist *worklist)
{
    MVMHamtNodeREPRData *repr_data = (MVMHamtNodeREPRData *)st->REPR_data;
    if (repr_data == NULL)
        return;
    // MVM_gc_worklist_add(tc, worklist, &repr_data->elem_type);
}

/* Frees the representation data in an STable.*/
static void gc_free_repr_data(MVMThreadContext *tc, MVMSTable *st)
{
    MVM_free(st->REPR_data);
}

static const MVMStorageSpec storage_spec = {
    0,                          /* inlineable */
    0,                          /* bits */
    0,                          /* align */
    MVM_STORAGE_SPEC_BP_NONE,   /* boxed_primitive */
    0,                          /* can_box */
    0,                          /* is_unsigned */
};

/* Gets the storage specification for this representation. */
static const MVMStorageSpec *get_storage_spec(MVMThreadContext *tc,
                                              MVMSTable *st)
{
    return &storage_spec;
}

void MVM_VMArray_at_pos(MVMThreadContext *tc, MVMSTable *st, MVMObject *root,
                        void *data, MVMint64 index, MVMRegister *value,
                        MVMuint16 kind)
{
    MVMHamtNodeREPRData *repr_data = (MVMHamtNodeREPRData *)st->REPR_data;

}


void MVM_VMArray_bind_pos(MVMThreadContext *tc, MVMSTable *st, MVMObject *root,
                          void *data, MVMint64 index, MVMRegister value,
                          MVMuint16 kind)
{
    MVMHamtNodeREPRData *repr_data = (MVMHamtNodeREPRData *)st->REPR_data;
    MVMHamtNodeBody *body = (MVMHamtNodeBody *)data;
    MVMuint64 real_index;
}

// static MVMStorageSpec get_elem_storage_spec(MVMThreadContext *tc, MVMSTable *st)
// {
//     MVMHamtNodeREPRData *repr_data = (MVMHamtNodeREPRData *)st->REPR_data;
//     MVMStorageSpec spec;

//     /* initialise storage spec to default values */
//     spec.bits = 0;
//     spec.align = 0;
//     spec.is_unsigned = 0;

//     switch (repr_data->slot_type) {
//     case MVM_ARRAY_STR:
//         spec.inlineable = MVM_STORAGE_SPEC_INLINED;
//         spec.boxed_primitive = MVM_STORAGE_SPEC_BP_STR;
//         spec.can_box = MVM_STORAGE_SPEC_CAN_BOX_STR;
//         break;
//     case MVM_ARRAY_I64:
//     case MVM_ARRAY_I32:
//     case MVM_ARRAY_I16:
//     case MVM_ARRAY_I8:
//         spec.inlineable = MVM_STORAGE_SPEC_INLINED;
//         spec.boxed_primitive = MVM_STORAGE_SPEC_BP_INT;
//         spec.can_box = MVM_STORAGE_SPEC_CAN_BOX_INT;
//         break;
//     case MVM_ARRAY_N64:
//     case MVM_ARRAY_N32:
//         spec.inlineable = MVM_STORAGE_SPEC_INLINED;
//         spec.boxed_primitive = MVM_STORAGE_SPEC_BP_NUM;
//         spec.can_box = MVM_STORAGE_SPEC_CAN_BOX_NUM;
//         break;
//     case MVM_ARRAY_U64:
//     case MVM_ARRAY_U32:
//     case MVM_ARRAY_U16:
//     case MVM_ARRAY_U8:
//         spec.inlineable = MVM_STORAGE_SPEC_INLINED;
//         spec.boxed_primitive = MVM_STORAGE_SPEC_BP_UINT64;
//         spec.can_box = MVM_STORAGE_SPEC_CAN_BOX_INT;
//         spec.is_unsigned = 1;
//         break;
//     default:
//         spec.inlineable = MVM_STORAGE_SPEC_REFERENCE;
//         spec.boxed_primitive = MVM_STORAGE_SPEC_BP_NONE;
//         spec.can_box = 0;
//         break;
//     }
//     return spec;
// }

/* Compose the representation. */
static void spec_to_repr_data(MVMThreadContext *tc,
                              MVMHamtNodeREPRData *repr_data,
                              const MVMStorageSpec *spec)
{
}
static void compose(MVMThreadContext *tc, MVMSTable *st, MVMObject *info_hash)
{
}

/* Set the size of the STable. */
static void deserialize_stable_size(MVMThreadContext *tc, MVMSTable *st,
                                    MVMSerializationReader *reader)
{
    st->size = sizeof(MVMHamtNode);
}

/* Serializes the REPR data. */
static void serialize_repr_data(MVMThreadContext *tc, MVMSTable *st,
                                MVMSerializationWriter *writer)
{
    // MVMHamtNodeREPRData *repr_data = (MVMHamtNodeREPRData *)st->REPR_data;
    // MVM_serialization_write_ref(tc, writer, repr_data->elem_type);
}

/* Deserializes representation data. */
static void deserialize_repr_data(MVMThreadContext *tc, MVMSTable *st,
                                  MVMSerializationReader *reader)
{
}

static void deserialize(MVMThreadContext *tc, MVMSTable *st, MVMObject *root,
                        void *data, MVMSerializationReader *reader)
{
    MVMHamtNodeREPRData *repr_data = (MVMHamtNodeREPRData *)st->REPR_data;
    MVMHamtNodeBody *body = (MVMHamtNodeBody *)data;
    MVMuint64 i;
}

static void serialize(MVMThreadContext *tc, MVMSTable *st, void *data,
                      MVMSerializationWriter *writer)
{
    MVMHamtNodeREPRData *repr_data = (MVMHamtNodeREPRData *)st->REPR_data;
    MVMHamtNodeBody *body = (MVMHamtNodeBody *)data;
    MVMuint64 i;
}

/* Calculates the non-GC-managed memory we hold on to. */
static MVMuint64 unmanaged_size(MVMThreadContext *tc, MVMSTable *st, void *data)
{
    // MVMHamtNodeREPRData *repr_data = (MVMHamtNodeREPRData *)st->REPR_data;
    // MVMHamtNodeBody *body = (MVMHamtNodeBody *)data;
    // return body->ssize * repr_data->elem_size;
}

static void describe_refs(MVMThreadContext *tc, MVMHeapSnapshotState *ss,
                          MVMSTable *st, void *data)
{
    // MVMHamtNodeREPRData *repr_data = (MVMHamtNodeREPRData *)st->REPR_data;
    // MVMHamtNodeBody *body = (MVMHamtNodeBody *)data;
    // MVMuint64 elems = body->elems;
    // MVMuint64 start = body->start;
    // MVMuint64 i = 0;

    // switch (repr_data->slot_type) {
    // case MVM_ARRAY_OBJ: {
    //     MVMObject **slots = body->slots.o;
    //     slots += start;
    //     while (i < elems) {
    //         MVM_profile_heap_add_collectable_rel_idx(
    //             tc, ss, (MVMCollectable *)slots[i], i);
    //         i++;
    //     }
    //     break;
    // }
    // case MVM_ARRAY_STR: {
    //     MVMString **slots = body->slots.s;
    //     slots += start;
    //     while (i < elems) {
    //         MVM_profile_heap_add_collectable_rel_idx(
    //             tc, ss, (MVMCollectable *)slots[i], i);
    //         i++;
    //     }
    //     break;
    // }
    // }
}

/* Initializes the representation. */
const MVMREPROps *MVMHamtNode_initialize(MVMThreadContext *tc)
{
    return &VMArray_this_repr;
}

static const MVMREPROps VMArray_this_repr = {
    type_object_for,
    MVM_gc_allocate_object, /* serialization.c relies on this and the next line
                             */
    NULL,                   /* initialize */
    copy_to,
    MVM_REPR_DEFAULT_ATTR_FUNCS,
    MVM_REPR_DEFAULT_BOX_FUNCS,
    {MVM_VMArray_at_pos, MVM_VMArray_bind_pos, MVM_REPR_DEFAULT_SET_ELEMS,
     MVM_REPR_DEFAULT_PUSH, MVM_REPR_DEFAULT_POP, MVM_REPR_DEFAULT_UNSHIFT,
     MVM_REPR_DEFAULT_SHIFT, MVM_REPR_DEFAULT_SLICE, MVM_REPR_DEFAULT_SPLICE,
     MVM_REPR_DEFAULT_AT_POS_MULTIDIM, MVM_REPR_DEFAULT_BIND_POS_MULTIDIM,
     MVM_REPR_DEFAULT_DIMENSIONS, MVM_REPR_DEFAULT_SET_DIMENSIONS,
     MVM_REPR_DEFAULT_GET_ELEM_STORAGE_SPEC, MVM_REPR_DEFAULT_POS_AS_ATOMIC,
     MVM_REPR_DEFAULT_POS_AS_ATOMIC_MULTIDIM, MVM_REPR_DEFAULT_POS_WRITE_BUF,
     MVM_REPR_DEFAULT_POS_READ_BUF

    }, /* pos_funcs */
    MVM_REPR_DEFAULT_ASS_FUNCS,
    NULL /* elems */,
    NULL, /* get_storage_spec */
    NULL, /* change_type */
    serialize,
    deserialize,
    serialize_repr_data,
    deserialize_repr_data,
    deserialize_stable_size,
    VMArray_gc_mark,
    gc_free,
    NULL, /* gc_cleanup */
    gc_mark_repr_data,
    gc_free_repr_data,
    compose,
    NULL,
    "VMAHamtNode", /* name */
    -1 , /* place holder for REPR ID, will be set in MVM_repr_register_dynamic_repr() */
    unmanaged_size,
    describe_refs,
};
