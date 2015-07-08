#ifndef ___PB_CPP_API__H___
#define ___PB_CPP_API__H___

#include <pb_decode.h>
#include <pb_encode.h>


namespace nanopb {

template <typename Obj, typename Fields>
inline UInt8Array serialize_to_array(Obj &obj, Fields &fields,
                                     UInt8Array output) {
  pb_ostream_t ostream = pb_ostream_from_buffer(output.data, output.length);
  bool ok = pb_encode(&ostream, fields, &obj);
  if (ok) {
    output.length = ostream.bytes_written;
  } else {
    output.length = 0;
    output.data = NULL;
  }
  return output;
}


template <typename Fields, typename Obj>
inline bool decode_from_array(UInt8Array input, Fields const &fields, Obj &obj,
                              bool init_default=false) {
  pb_istream_t istream = pb_istream_from_buffer(input.data, input.length);
  if (init_default) {
    return pb_decode(&istream, fields, &obj);
  } else {
    return pb_decode_noinit(&istream, fields, &obj);
  }
}


template <typename Msg>
inline Msg get_pb_default(const pb_field_t *fields) {
  /* Use nanopb decode with `init_default` set to `true` as a workaround to
   * avoid needing to initialize the object (i.e., `_`) using the
   * `<message>_init_default` macro.
   *
   * This is necessary because the macro resolves to an initialization list,
   * which cannot be referenced in a memory efficient, generalizable way. */
  Msg obj;
  UInt8Array null_array;
  null_array.length = 0;
  decode_from_array(null_array, fields, obj, true);
  return obj;
}


template <typename Msg, typename Validator>
class Message {
public:
  Msg _;
  Validator validator_;
  UInt8Array buffer_;
  const pb_field_t *fields_;

  Message(const pb_field_t *fields) : fields_(fields) {}

  Message(const pb_field_t *fields, size_t buffer_size, uint8_t *buffer)
    : fields_(fields) {
    buffer_.length = buffer_size;
    buffer_.data = buffer;
  }

  Message(const pb_field_t *fields, UInt8Array buffer)
    : fields_(fields), buffer_(buffer) {}

  void set_buffer(UInt8Array buffer) { buffer_ = buffer; }

  void reset() { _ = get_pb_default<Msg>(fields_); }
  uint8_t update(UInt8Array serialized) {
    Msg &obj = *((Msg *)buffer_.data);
    bool ok = decode_from_array(serialized, fields_, obj, true);
    if (ok) {
      validator_.update(fields_, obj, _);
    }
    return ok;
  }
  UInt8Array serialize() {
    return serialize_to_array(_, fields_, buffer_);
  }
  void validate() {
    Msg &obj = *((Msg *)buffer_.data);
    obj = get_pb_default<Msg>(fields_);
    /* Validate the active configuration structure (i.e., trigger the
     * validation callbacks). */
    validator_.update(fields_, _, obj);
  }
};


} // namespace nanopb


#endif  // #ifndef ___PB_CPP_API__H___
