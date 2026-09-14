#include <cassert>
#include <cstddef>
#include <cstdint>

#include <ESPressio_WebPrimitiveSchema.hpp>
#include <ESPressio_SerializableCommand.hpp>
#include <ESPressio_SerializationMacros.hpp>
#include <ESPressio_TypeDirectory.hpp>

using namespace ESPressio;
using namespace ESPressio::Web;

namespace {

const Serializable::StaticSchemaDescriptor PrimarySchema{
    1, 1, 1, 0, nullptr, 0x1234u, 8, 16, 32
};
const Serializable::StaticSchemaDescriptor SecondarySchema{
    1, 1, 1, 0, nullptr, 0x5678u, 4, 8, 16
};

struct SchemaCommand final : Command::SerializableCommand<SchemaCommand> {
    static constexpr Command::CommandTypeId TypeId{201};
    static constexpr const char* CanonicalName="Test.Web.SchemaCommand";
    static constexpr std::size_t MaximumLiveInstances=2;
    static constexpr std::size_t MaximumPendingExecutions=1;
    using ExecutionAdmissionPolicy=Command::RequiredExecution;
    std::int32_t Value=0;
    ESPRESSIO_SERIALIZABLE_TYPE(SchemaCommand)
    ESPRESSIO_SERIALIZABLE_SCHEMA_VERSION(1)
    ESPRESSIO_SERIALIZABLE_PROPERTIES(ESPRESSIO_PROPERTY("value",Value))
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

void TestRealSerializableCommandDescriptor() {
    const auto descriptor=SchemaCommand::GetPrimitiveTypeDescriptor();
    assert(descriptor.IsValid());
    assert(descriptor.Capabilities.Contains(Primitive::PrimitiveTypeCapability::Serializable));
    assert(descriptor.SerializedSize.MaximumCompletePrimitiveWireBytes>0);

    Primitive::TypeDirectory<1> directory;
    assert(directory.Register<SchemaCommand>()==Primitive::TypeDirectoryRegistrationStatus::Success);
    assert(directory.Initialize()==Primitive::TypeDirectoryInitializationStatus::Success);
    const auto view=directory.View();
    const auto* discovered=view.Find({Primitive::FamilyIds::Command,SchemaCommand::TypeId.Value()});
    assert(discovered);

    PrimitiveSchemaMetadata metadata;
    const auto result=DescribePrimitiveSchema(*discovered,metadata);
    assert(result);
    assert(metadata.PrimarySchema!=nullptr);
    assert(metadata.DynamicCapability==PrimitiveDynamicCapability::CommandFireAndForget);
    assert(metadata.MaximumCompletePrimitiveWireBytes==descriptor.SerializedSize.MaximumCompletePrimitiveWireBytes);
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
    TestRealSerializableCommandDescriptor();
    TestStateIsReadOnly();
    TestFailClosedFamilyInterpretation();
    return 0;
}
