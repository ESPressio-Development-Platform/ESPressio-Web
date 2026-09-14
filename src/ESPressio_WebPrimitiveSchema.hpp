#pragma once

#if !__has_include(<ESPressio_PrimitiveTypeDescriptor.hpp>) || \
    !__has_include(<ESPressio_SchemaDescriptor.hpp>) || \
    !__has_include(<ESPressio_EventTypeDescriptor.hpp>) || \
    !__has_include(<ESPressio_CommandDescriptor.hpp>) || \
    !__has_include(<ESPressio_StateDescriptor.hpp>)
#error "ESPressio Web Primitive schema integration requires Primitive, Serializable, Event, Command and State."
#endif

#include <array>
#include <cstddef>
#include <cstdint>

#include <ESPressio_PrimitiveTypeDescriptor.hpp>
#include <ESPressio_SchemaDescriptor.hpp>
#include <ESPressio_EventTypeDescriptor.hpp>
#include <ESPressio_CommandDescriptor.hpp>
#include <ESPressio_StateDescriptor.hpp>

#include "ESPressio_WebTypes.hpp"

namespace ESPressio::Web {

/// <summary>
/// Family-owned dynamic capability exposed to Web tooling after P1 discovery.
/// </summary>
enum class PrimitiveDynamicCapability : std::uint8_t {
    MetadataOnly,
    EventDispatch,
    CommandFireAndForget,
    CommandRequesterRequired,
    StateReadOnly
};

/// <summary>
/// Borrowed immutable P3/family metadata for one frozen Primitive descriptor.
/// </summary>
/// <remarks>
/// Web stores no family callbacks here and does not interpret FamilyExtension
/// itself. Event/Command/State validate and interpret their own extension first.
/// Availability describes capability, not authorization to perform an operation.
/// </remarks>
struct PrimitiveSchemaMetadata final {
    Primitive::PrimitiveTypeKey Key{};
    Primitive::PrimitiveTypeNameView CanonicalName{};
    Primitive::PrimitiveTypeCapabilities Capabilities{};
    const Serializable::StaticSchemaDescriptor* PrimarySchema = nullptr;
    const Serializable::StaticSchemaDescriptor* SecondarySchema = nullptr;
    PrimitiveDynamicCapability DynamicCapability = PrimitiveDynamicCapability::MetadataOnly;
    std::size_t MaximumCompletePrimitiveWireBytes = 0;
};

/// <summary>
/// Resolves P3/schema and constructibility facts through the owning family.
/// </summary>
inline WebResult DescribePrimitiveSchema(
    const Primitive::PrimitiveTypeDescriptor& common,
    PrimitiveSchemaMetadata& metadata) noexcept {
    metadata = {};
    if (!common.IsValid()) return WebResult::Failure(WebError::InvalidConfiguration);

    metadata.Key = common.Key;
    metadata.CanonicalName = common.CanonicalName;
    metadata.Capabilities = common.Capabilities;
    metadata.MaximumCompletePrimitiveWireBytes =
        common.SerializedSize.MaximumCompletePrimitiveWireBytes;

    switch (common.Key.Family) {
        case Primitive::FamilyIds::Event: {
            const auto* event = Event::GetEventTypeDescriptor(common);
            if (!event) return WebResult::Failure(WebError::ProtocolError);
            metadata.PrimarySchema = event->Schema;
            if (event->DynamicallyConstructible && event->DispatchSerialized != nullptr)
                metadata.DynamicCapability = PrimitiveDynamicCapability::EventDispatch;
            return WebResult::Success();
        }
        case Primitive::FamilyIds::Command: {
            const auto* command = Command::GetCommandTypeDescriptor(common);
            if (!command) return WebResult::Failure(WebError::ProtocolError);
            metadata.PrimarySchema = command->RequestSchema;
            metadata.SecondarySchema = command->ResponseSchema;
            switch (command->DynamicConstruction) {
                case Command::CommandDynamicConstructionMode::FireAndForget:
                    metadata.DynamicCapability = command->SubmitSerialized
                        ? PrimitiveDynamicCapability::CommandFireAndForget
                        : PrimitiveDynamicCapability::MetadataOnly;
                    break;
                case Command::CommandDynamicConstructionMode::RequesterRequired:
                    metadata.DynamicCapability = PrimitiveDynamicCapability::CommandRequesterRequired;
                    break;
                case Command::CommandDynamicConstructionMode::Unavailable:
                    break;
            }
            return WebResult::Success();
        }
        case Primitive::FamilyIds::State: {
            const auto* state = State::GetStateTypeDescriptor(common);
            if (!state) return WebResult::Failure(WebError::ProtocolError);
            metadata.PrimarySchema = state->ValueSchema;
            for (const auto read : state->ReadValue) {
                if (read != nullptr) {
                    metadata.DynamicCapability = PrimitiveDynamicCapability::StateReadOnly;
                    break;
                }
            }
            return WebResult::Success();
        }
        default:
            // P1 still exposes common metadata for private/other families. Web
            // cannot interpret a family-owned P3 extension without that family.
            return WebResult::Failure(WebError::Unsupported);
    }
}

} // namespace ESPressio::Web
