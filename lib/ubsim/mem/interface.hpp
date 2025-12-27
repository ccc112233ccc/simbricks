#ifndef UBSIM_MEM_INTERFACE_HPP_
#define UBSIM_MEM_INTERFACE_HPP_

#include <cstdint>
#include <variant>

#include <ubsim/mem/if.h>
#include <ubsim/mem/zmq.h>

namespace ubsim {

inline BaseIfParams DefaultMemParams() { return MemIf::DefaultParams(); }

class MemTransport {
 public:
  enum class Mode {
    kShm,
    kZmq,
  };

  explicit MemTransport(Mode mode) : mode_(mode) {
    if (mode_ == Mode::kShm) {
      transport_.emplace<MemIf>();
    } else {
      transport_.emplace<ZmqMemIf>();
    }
  }

  Mode mode() const { return mode_; }

  MemIf &shm() { return std::get<MemIf>(transport_); }
  const MemIf &shm() const { return std::get<MemIf>(transport_); }
  ZmqMemIf &zmq() { return std::get<ZmqMemIf>(transport_); }
  const ZmqMemIf &zmq() const { return std::get<ZmqMemIf>(transport_); }

  volatile UbsimProtoMemH2M *H2MOutAlloc(uint64_t ts) {
    return std::visit(
        [ts](auto &impl) { return impl.H2MOutAlloc(ts); }, transport_);
  }

  void H2MOutSend(volatile UbsimProtoMemH2M *msg, uint8_t type) {
    std::visit([msg, type](auto &impl) { impl.H2MOutSend(msg, type); },
               transport_);
  }

  int H2MOutSync(uint64_t ts) {
    return std::visit([ts](auto &impl) { return impl.H2MOutSync(ts); },
                      transport_);
  }

  size_t H2MOutMsgLen() const {
    return std::visit([](const auto &impl) { return impl.H2MOutMsgLen(); },
                      transport_);
  }

  volatile UbsimProtoMemH2M *H2MInPoll(uint64_t ts) {
    return std::visit(
        [ts](auto &impl) { return impl.H2MInPoll(ts); }, transport_);
  }

  uint8_t H2MInType(volatile UbsimProtoMemH2M *msg) const {
    return std::visit([msg](const auto &impl) { return impl.H2MInType(msg); },
                      transport_);
  }

  void H2MInDone(volatile UbsimProtoMemH2M *msg) {
    std::visit([msg](auto &impl) { impl.H2MInDone(msg); }, transport_);
  }

  uint64_t H2MInTimestamp() const {
    return std::visit([](const auto &impl) { return impl.H2MInTimestamp(); },
                      transport_);
  }

  volatile UbsimProtoMemM2H *M2HOutAlloc(uint64_t ts) {
    return std::visit(
        [ts](auto &impl) { return impl.M2HOutAlloc(ts); }, transport_);
  }

  void M2HOutSend(volatile UbsimProtoMemM2H *msg, uint8_t type) {
    std::visit([msg, type](auto &impl) { impl.M2HOutSend(msg, type); },
               transport_);
  }

  volatile UbsimProtoMemM2H *M2HInPoll(uint64_t ts) {
    return std::visit(
        [ts](auto &impl) { return impl.M2HInPoll(ts); }, transport_);
  }

  uint8_t M2HInType(volatile UbsimProtoMemM2H *msg) const {
    return std::visit([msg](const auto &impl) { return impl.M2HInType(msg); },
                      transport_);
  }

  void M2HInDone(volatile UbsimProtoMemM2H *msg) {
    std::visit([msg](auto &impl) { impl.M2HInDone(msg); }, transport_);
  }

  uint64_t M2HInTimestamp() const {
    return std::visit([](const auto &impl) { return impl.M2HInTimestamp(); },
                      transport_);
  }

 private:
  Mode mode_;
  std::variant<MemIf, ZmqMemIf> transport_;
};

template <typename T>
inline volatile UbsimProtoMemH2M *H2MOutAlloc(T *mem, uint64_t ts) {
  return mem->H2MOutAlloc(ts);
}

template <typename T>
inline void H2MOutSend(T *mem, volatile UbsimProtoMemH2M *msg, uint8_t type) {
  mem->H2MOutSend(msg, type);
}

template <typename T>
inline int H2MOutSync(T *mem, uint64_t ts) {
  return mem->H2MOutSync(ts);
}

template <typename T>
inline size_t H2MOutMsgLen(T *mem) {
  return mem->H2MOutMsgLen();
}

template <typename T>
inline volatile UbsimProtoMemH2M *H2MInPoll(T *mem, uint64_t ts) {
  return mem->H2MInPoll(ts);
}

template <typename T>
inline uint8_t H2MInType(T *mem, volatile UbsimProtoMemH2M *msg) {
  return mem->H2MInType(msg);
}

template <typename T>
inline void H2MInDone(T *mem, volatile UbsimProtoMemH2M *msg) {
  mem->H2MInDone(msg);
}

template <typename T>
inline uint64_t H2MInTimestamp(T *mem) {
  return mem->H2MInTimestamp();
}

template <typename T>
inline volatile UbsimProtoMemM2H *M2HOutAlloc(T *mem, uint64_t ts) {
  return mem->M2HOutAlloc(ts);
}

template <typename T>
inline void M2HOutSend(T *mem, volatile UbsimProtoMemM2H *msg, uint8_t type) {
  mem->M2HOutSend(msg, type);
}

template <typename T>
inline volatile UbsimProtoMemM2H *M2HInPoll(T *mem, uint64_t ts) {
  return mem->M2HInPoll(ts);
}

template <typename T>
inline uint8_t M2HInType(T *mem, volatile UbsimProtoMemM2H *msg) {
  return mem->M2HInType(msg);
}

template <typename T>
inline void M2HInDone(T *mem, volatile UbsimProtoMemM2H *msg) {
  mem->M2HInDone(msg);
}

template <typename T>
inline uint64_t M2HInTimestamp(T *mem) {
  return mem->M2HInTimestamp();
}

}  // namespace ubsim

#endif  // UBSIM_MEM_INTERFACE_HPP_
