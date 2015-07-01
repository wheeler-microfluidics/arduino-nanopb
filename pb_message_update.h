#ifndef ___PB_UPDATE_MESSAGE__H___
#define ___PB_UPDATE_MESSAGE__H___

#include <pb_common.h>

#ifdef LOGGING
#define LOG(...) printf(__VA_ARGS__)
#else
#define LOG(...)
#endif


struct MessageUpdateBase {
  /* Define behaviour to update an existing nano-protocol buffer message
   * `struct`, based on the contents of a (possibly partial) secondary message.
   * */

  struct IterPair {
    pb_field_iter_t source;  /* Reference to field in source struct. */
    pb_field_iter_t target;  /* Reference to field in target struct. */
  };

  struct Parent {
    const pb_field_t *start;  /* Start of the pb_field_t array */
    const pb_field_t *pos;  /* Current position of the iterator */
  };

  Parent parents[5];
  pb_size_t parent_count;

  MessageUpdateBase() { reset(); }

  void reset() { parent_count = 0; }

  template <typename Fields, typename Message>
  void update(Fields fields, Message &source, Message &target) {
    reset();
    LOG("\n**************************************************\n");
    __update__(fields, (void *)&source, (void *)&target);
  }

  virtual bool process_field(IterPair &iter, pb_size_t count) = 0;
private:

  template <typename Iter>
  pb_size_t extract_count(Iter &iter) {
    pb_type_t type = iter.pos->type;
    pb_size_t count = 0;

    /* Load the count of the data for the current field in the source
      * structure. */
    if (PB_HTYPE(type) == PB_HTYPE_OPTIONAL && *(bool*)(iter.pSize)) {
      count = 1;
    } else if (PB_HTYPE(type) == PB_HTYPE_REPEATED) {
      count = *(pb_size_t*)(iter.pSize);
    } else if (PB_HTYPE(type) == PB_HTYPE_REQUIRED) {
      count = 1;
    }
    return count;
  }

  template <typename Fields>
  void __update__(Fields fields, void *source, void *target) {
    /* Iterate through each field in the Protocol Buffer message defined by
     * `fields`.
     *
     * For each field, call `process_field` method to assess whether or not the
     * corresponding data from the source structure should be copied to the
     * target structure. */
    IterPair iter;

    if (!pb_field_iter_begin(&iter.source, fields, source)) {
      return; /* Empty message type */
    }

    if (!pb_field_iter_begin(&iter.target, fields, target)) {
      return; /* Empty message type */
    }

    do {
      pb_type_t type;
      pb_size_t count = 0;
      pb_size_t target_count = 0;
      type = iter.source.pos->type;

      /* TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO */
      /* TODO Define behaviour for non-static types, e.g., repeated. */
      /* TODO  - Repeated:
       *        * Copy up to count.
       *        * Do not process sub-message.
       *        * Set new count. */
      /* TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO */

      if (PB_ATYPE(type) == PB_ATYPE_STATIC) {
        count = extract_count(iter.source);
        target_count = extract_count(iter.target);

        int size = -10;
        /* If the `process_field` method returns `true`... */
        if (process_field(iter, count)) {
          /*  - Copy data from source structure to target structure. */
          memcpy(iter.target.pData, iter.source.pData,
                 iter.source.pos->data_size);
          /*  - Update the `has_` field or the `_count` field of the target
           *    structure. */
          if (PB_HTYPE(type) == PB_HTYPE_OPTIONAL) {
            *(bool*)iter.target.pSize = *(bool*)iter.source.pSize;
            size = *(bool*)iter.target.pSize;
          } else if (PB_HTYPE(type) == PB_HTYPE_REPEATED) {
            *(pb_size_t*)iter.target.pSize = *(pb_size_t*)iter.source.pSize;
            size = *(pb_size_t*)iter.target.pSize;
          } else {
            size = -20;
          }
        }
        LOG(">> source_count=%d, target_count=%d (size: %d, LTYPE=%d)\n",
               count, target_count, size, PB_LTYPE(iter.source.pos->type));

        /* If the current field is a sub-message, push parent message onto
         * parent stack and process sub-message fields. */
        if ((count > 0) && (PB_LTYPE(iter.source.pos->type) ==
                            PB_LTYPE_SUBMESSAGE)) {
          /*  - Mark sub-message types as present if they are present in the
           *    source message. */
          *(bool*)iter.target.pSize = *(bool*)iter.source.pSize;

          parents[parent_count].pos = iter.source.pos;
          parents[parent_count].start = iter.source.start;
          parent_count++;
          for (int i = 0; i < count; i++) {
            /* For repeated types, the count may be greater than one.  In such
             * cases, we need to iterate through each repeated sub-message to
             * handle each one separately.
             *
             * Note that we cast `pData` from a `void` pointer to a `uint8_t`
             * to avoid [compiler warnings][1] about pointer arithmetic
             * involving a `void` pointer.
             *
             * [1]: http://stackoverflow.com/questions/26755638/warning-pointer-of-type-void-used-in-arithmetic#26756220 */
            pb_size_t offset = i * iter.source.pos->data_size;
            __update__((Fields)iter.source.pos->ptr,
                       ((uint8_t *)iter.source.pData) + offset,
                       ((uint8_t *)iter.target.pData) + offset);
          }
          --parent_count;
        }
        count = extract_count(iter.source);
        target_count = extract_count(iter.target);
        LOG("<< source_count=%d, target_count=%d\n", count, target_count);
      } else {
        LOG("Unrecognized PB_ATYPE=%d\n", PB_ATYPE(type));
      }
    } while (pb_field_iter_next(&iter.source) && pb_field_iter_next(&iter.target));
  }
};


struct MessageUpdate : public MessageUpdateBase {
  using MessageUpdateBase::IterPair;

  MessageUpdate() : MessageUpdateBase() {}

  virtual bool process_field(IterPair &iter, pb_size_t count) {
    for (int i = 0; i < parent_count; i++) LOG("  ");
    LOG("=========================================\n");
    for (int i = 0; i < parent_count; i++) LOG("  ");
    if (parent_count > 0) {
      for (int i = 0; i < parent_count; i++) {
        LOG("> %d ", parents[i].pos->tag);
      }
      LOG("\n");
    }
    for (int i = 0; i < parent_count; i++) LOG("  ");
    LOG("tag=%d start=%p pos=%p count=%d ltype=%x atype=%x htype=%x data_size=%d \n",
           iter.source.pos->tag, iter.source.start, iter.source.pos, count,
           PB_LTYPE(iter.source.pos->type), PB_ATYPE(iter.source.pos->type),
           PB_HTYPE(iter.source.pos->type), iter.source.pos->data_size);
    for (int i = 0; i < parent_count; i++) LOG("  ");
    LOG("-----------------------------------------");

    bool trigger_copy = false;
    if (PB_LTYPE(iter.source.pos->type) != PB_LTYPE_SUBMESSAGE) {
      /* Only copy all data for field if this is not a sub-message type, since
       * we want to handle sub-message fields one-by-one. */
      trigger_copy = (count > 0);
    }
    LOG("%c\n", (trigger_copy) ? 'W' : ' ');
    return trigger_copy;
  }
};


#endif  // #ifndef ___PB_UPDATE_MESSAGE__H___
