#pragma once

#if !__has_include(<ESPressio_StatePublisher.hpp>) || !__has_include(<ESPressio_StateCodec.hpp>)
#error "ESPressio Web State integration requires ESPressio-State."
#endif

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

#include <ESPressio_Memory.hpp>
#include <ESPressio_StateCodec.hpp>
#include <ESPressio_StatePublisher.hpp>

#include "ESPressio_Router.hpp"

namespace ESPressio::Web {

namespace StateHttpHeaderName {
inline constexpr std::string_view Type = "X-ESPressio-State-Type";
inline constexpr std::string_view TypeId = "X-ESPressio-State-Type-Id";
inline constexpr std::string_view Epoch = "X-ESPressio-State-Epoch";
inline constexpr std::string_view Revision = "X-ESPressio-State-Revision";
} // namespace StateHttpHeaderName

namespace Detail {
template<typename TDefinition, typename = void>
struct HttpStateDefinitionName final {
    static constexpr const char* Value = nullptr;
};

template<typename TDefinition>
struct HttpStateDefinitionName<
    TDefinition,
    std::void_t<decltype(TDefinition::Name)>
> final {
    static_assert(
        std::is_convertible_v<decltype(TDefinition::Name), const char*>,
        "State definition Name must be convertible to const char*"
    );
    static constexpr const char* Value = TDefinition::Name;
};
} // namespace Detail

template<typename TDefinition>
class IHttpStateSnapshotRepresentation {
public:
    using Value = State::StateValueType<TDefinition>;
    using Update = State::StateUpdate<Value>;

    virtual ~IHttpStateSnapshotRepresentation() = default;
    virtual WebResult Write(
        const Update& update,
        WebRequestContext& context
    ) = 0;
};

template<typename TDefinition>
class StateCodecHttpSnapshotRepresentation final :
    public IHttpStateSnapshotRepresentation<TDefinition> {
private:
    using PayloadBuffer = System::Memory::Vector<
        uint8_t,
        System::Memory::MemoryPolicy::ExternalPreferred
    >;

public:
    using Value = State::StateValueType<TDefinition>;
    using Update = State::StateUpdate<Value>;

    WebResult Write(
        const Update& update,
        WebRequestContext& context
    ) override {
        PayloadBuffer payload(State::StateCodec<TDefinition>::MaximumEncodedSize);
        std::size_t payloadSize = 0;
        if (!State::StateCodec<TDefinition>::Encode(
                update.Value,
                payload.data(),
                payload.size(),
                payloadSize)) {
            return WebResult::Failure(WebError::ProtocolError);
        }
        if (payloadSize > payload.size()) {
            return WebResult::Failure(WebError::ProtocolError);
        }

        auto& response = context.Response();
        auto result = response.Status(HttpStatus::Ok);
        if (!result) return result;
        result = response.ContentType("application/octet-stream");
        if (!result) return result;

        constexpr const char* typeName =
            Detail::HttpStateDefinitionName<TDefinition>::Value;
        if constexpr (typeName != nullptr) {
            result = response.Header(StateHttpHeaderName::Type, typeName);
            if (!result) return result;
        }

        result = NumericHeader(
            response,
            StateHttpHeaderName::TypeId,
            update.Header.TypeId
        );
        if (!result) return result;
        result = NumericHeader(
            response,
            StateHttpHeaderName::Epoch,
            update.Header.Epoch
        );
        if (!result) return result;
        result = NumericHeader(
            response,
            StateHttpHeaderName::Revision,
            update.Header.Revision
        );
        if (!result) return result;

        result = response.Begin(payloadSize);
        if (!result) return result;
        if (context.Request().Method() != HttpMethod::Head && payloadSize != 0) {
            result = response.Write(payload.data(), payloadSize);
            if (!result) {
                response.Abort();
                return result;
            }
        }
        return response.Complete();
    }

private:
    template<typename TValue>
    static WebResult NumericHeader(
        HttpResponse& response,
        std::string_view name,
        TValue value
    ) {
        std::array<char, 32> buffer{};
        const auto converted = std::to_chars(
            buffer.data(),
            buffer.data() + buffer.size(),
            value
        );
        if (converted.ec != std::errc{}) {
            return WebResult::Failure(WebError::ProtocolError);
        }
        return response.Header(
            name,
            std::string_view(
                buffer.data(),
                static_cast<std::size_t>(converted.ptr - buffer.data())
            )
        );
    }
};

template<typename TContract, typename TDefinition>
class StateSnapshotHttpHandler final : public IHttpRouteHandler {
public:
    using Publisher = State::StatePublisher<TContract>;
    using Value = State::StateValueType<TDefinition>;
    using Update = State::StateUpdate<Value>;

    static_assert(
        TContract::template Contains<TDefinition>,
        "State definition is not part of this StateContract"
    );

    explicit StateSnapshotHttpHandler(
        Publisher& publisher,
        IHttpStateSnapshotRepresentation<TDefinition>* representation = nullptr
    ) : _publisher(publisher),
        _representation(
            representation == nullptr
                ? static_cast<IHttpStateSnapshotRepresentation<TDefinition>*>(
                    &_defaultRepresentation
                  )
                : representation
        ) {}

    HttpHandlerResult Handle(
        WebRequestContext& context,
        const RouteParameters&
    ) override {
        const auto method = context.Request().Method();
        if (method != HttpMethod::Get && method != HttpMethod::Head) {
            return HttpHandlerResult::NotHandled();
        }

        Update update;
        if (!_publisher.template Snapshot<TDefinition>(update)) {
            return HttpHandlerResult::Failure(WebError::NotFound);
        }

        return HttpHandlerResult::Handled(
            _representation->Write(update, context)
        );
    }

private:
    Publisher& _publisher;
    StateCodecHttpSnapshotRepresentation<TDefinition> _defaultRepresentation;
    IHttpStateSnapshotRepresentation<TDefinition>* _representation;
};

} // namespace ESPressio::Web
