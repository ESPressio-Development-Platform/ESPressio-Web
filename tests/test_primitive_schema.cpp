#include <cassert>
#include <cstddef>
#include <cstdint>

#include <ESPressio_WebPrimitiveSchema.hpp>

using namespace ESPressio;
using namespace ESPressio::Web;

namespace {

const Serializable::StaticSchemaDescriptor PrimarySchema{
    1, 1, 1, 0, nullptr, 0x1234u, 8, 16, 32
};
const Serializable::StaticSchemaDescriptor SecondarySchema{
    1, 1, 1, 0, nullptr, 0x5678u, 4, 8, 16
};

Event::EventDynamicDispatchResult EventSubmit(
    Event::EventPayloadFormat,
    const std::uint8_t*,
    std::size_t) noexcept {
    return {Event::EventDynamicDispatchStatus::Accepted, Primitive::ConceptualMessageId{1}};
}

Command::CommandSubmissionResult CommandSubmit(
    Command::CommandPayloadFormat,
    const std::uint8_t*,
    std::size_t) noexcept {
    return {Command::CommandSubmissionStatus::Accepted, Command::CommandId{1}};
}

State::StateDynamicReadResult StateRead(std::uint8_t*, std::size_t) {
    return {State::StateDynamicReadStatus::NoValue, 0, {}};
}

Primitive::PrimitiveTypeDescriptor Common(
    Primitive::PrimitiveFamilyId family,
    std::uint64_t type,
    Primitive::PrimitiveFamilyExtensionRef extension) {
    return {
        {family, type},
        "test.type",
        Primitive::PrimitiveTypeCapabilities{1},
        {1, 1},
        {},
        {32},
        extension
    };
}

void TestEventMetadata() {
    Event::EventTypeDescriptor extension{};
    extension.TypeId = Event::EventTypeId{11};
    extension.Schema = &PrimarySchema;
    extension.DynamicallyConstructible = true;
    extension.DispatchSerialized = &EventSubmit;

    PrimitiveSchemaMetadata metadata;
    const auto result = DescribePrimitiveSchema(
        Common(Primitive::FamilyIds::Event, 11, {&extension}),
        metadata);
    assert(result);
    assert(metadata.Key.Family == Primitive::FamilyIds::Event);
    assert(metadata.PrimarySchema == &PrimarySchema);
    assert(metadata.SecondarySchema == nullptr);
    assert(metadata.DynamicCapability == PrimitiveDynamicCapability::EventDispatch);
    assert(metadata.MaximumCompletePrimitiveWireBytes == 32);
}

void TestCommandMetadata() {
    Command::CommandTypeDescriptor extension{};
    extension.TypeId = Command::CommandTypeId{12};
    extension.RequestSchema = &PrimarySchema;
    extension.ResponseSchema = &SecondarySchema;
    extension.DynamicConstruction = Command::CommandDynamicConstructionMode::FireAndForget;
    extension.SubmitSerialized = &CommandSubmit;

    PrimitiveSchemaMetadata metadata;
    auto result = DescribePrimitiveSchema(
        Common(Primitive::FamilyIds::Command, 12, {&extension}),
        metadata);
    assert(result);
    assert(metadata.PrimarySchema == &PrimarySchema);
    assert(metadata.SecondarySchema == &SecondarySchema);
    assert(metadata.DynamicCapability == PrimitiveDynamicCapability::CommandFireAndForget);

    extension.DynamicConstruction = Command::CommandDynamicConstructionMode::RequesterRequired;
    extension.SubmitSerialized = nullptr;
    result = DescribePrimitiveSchema(
        Common(Primitive::FamilyIds::Command, 12, {&extension}),
        metadata);
    assert(result);
    assert(metadata.DynamicCapability == PrimitiveDynamicCapability::CommandRequesterRequired);
}

void TestStateIsReadOnly() {
    State::StateTypeDescriptor extension{};
    extension.TypeId = State::StateTypeId{13};
    extension.ValueSchema = &PrimarySchema;
    extension.ReadValue[0] = &StateRead;

    PrimitiveSchemaMetadata metadata;
    const auto result = DescribePrimitiveSchema(
        Common(Primitive::FamilyIds::State, 13, {&extension}),
        metadata);
    assert(result);
    assert(metadata.PrimarySchema == &PrimarySchema);
    assert(metadata.SecondarySchema == nullptr);
    assert(metadata.DynamicCapability == PrimitiveDynamicCapability::StateReadOnly);
}

void TestFailClosedFamilyInterpretation() {
    PrimitiveSchemaMetadata metadata;
    auto result = DescribePrimitiveSchema(
        Common(Primitive::FamilyIds::ApplicationPrivateFirst, 1, {}),
        metadata);
    assert(!result);
    assert(result.Error == WebError::Unsupported);

    Event::EventTypeDescriptor wrong{};
    wrong.TypeId = Event::EventTypeId{99};
    result = DescribePrimitiveSchema(
        Common(Primitive::FamilyIds::Event, 14, {&wrong}),
        metadata);
    assert(!result);
    assert(result.Error == WebError::ProtocolError);
}

} // namespace

int main() {
    TestEventMetadata();
    TestCommandMetadata();
    TestStateIsReadOnly();
    TestFailClosedFamilyInterpretation();
    return 0;
}
