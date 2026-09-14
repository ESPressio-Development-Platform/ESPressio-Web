#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string_view>
#include <vector>

#include <ESPressio_WebCommand.hpp>

using namespace ESPressio;
using namespace ESPressio::Web;

namespace {

constexpr std::string_view FireAndForgetName="Test.Web.FireAndForget";
constexpr std::string_view RequesterName="Test.Web.Requester";

const Serializable::StaticSchemaDescriptor RequestSchema{
    1,1,1,0,nullptr,0x1234u,8,12,24
};

Command::CommandSubmissionStatus NextSubmissionStatus=Command::CommandSubmissionStatus::Accepted;
Command::CommandPayloadFormat LastFormat=Command::CommandPayloadFormat::DirectBinary;
std::array<std::uint8_t,64> LastPayload{};
std::size_t LastPayloadBytes=0;
std::size_t SubmissionCount=0;

Command::CommandSubmissionResult Submit(
    Command::CommandPayloadFormat format,
    const std::uint8_t* payload,
    std::size_t size) noexcept {
    ++SubmissionCount;
    LastFormat=format;
    LastPayloadBytes=size;
    if(size<=LastPayload.size() && payload) std::memcpy(LastPayload.data(),payload,size);
    return {NextSubmissionStatus,Command::CommandId{31}};
}

const Command::CommandTypeDescriptor FireAndForgetExtension=[] {
    Command::CommandTypeDescriptor value{};
    value.TypeId=Command::CommandTypeId{71};
    value.RequestSchema=&RequestSchema;
    value.DynamicConstruction=Command::CommandDynamicConstructionMode::FireAndForget;
    value.SubmitSerialized=&Submit;
    return value;
}();

const Command::CommandTypeDescriptor RequesterExtension=[] {
    Command::CommandTypeDescriptor value{};
    value.TypeId=Command::CommandTypeId{72};
    value.RequestSchema=&RequestSchema;
    value.DynamicConstruction=Command::CommandDynamicConstructionMode::RequesterRequired;
    return value;
}();

Primitive::PrimitiveTypeDescriptor Common(
    Command::CommandTypeId type,
    std::string_view name,
    const Command::CommandTypeDescriptor& extension) {
    return {
        {Command::CommandFamilyId,type.Value()},
        name,
        Primitive::PrimitiveTypeCapabilities{1},
        {1,1},
        {},
        {24},
        {&extension}
    };
}

class Request final : public IHttpRequestPlatform {
public:
    HttpMethod MethodValue=HttpMethod::Post;
    std::vector<std::uint8_t> Body;
    std::size_t Offset=0;
    std::optional<std::size_t> DeclaredLength;
    std::string_view ContentType;

    HttpMethod Method() const noexcept override { return MethodValue; }
    std::string_view Path() const noexcept override { return "/app-owned-command-route"; }
    std::string_view QueryString() const noexcept override { return {}; }
    std::optional<std::size_t> ContentLength() const noexcept override { return DeclaredLength; }
    bool HasHeader(std::string_view name) const noexcept override {
        return name==HttpHeaderName::ContentType && !ContentType.empty();
    }
    std::size_t HeaderValueLength(std::string_view name) const noexcept override {
        return HasHeader(name)?ContentType.size():0;
    }
    WebResult ReadHeader(std::string_view name,char* destination,std::size_t capacity,std::size_t& written) const override {
        written=0;
        if(!HasHeader(name)) return WebResult::Failure(WebError::NotFound);
        if(capacity<ContentType.size()) return WebResult::Failure(WebError::ResourceExhausted);
        std::memcpy(destination,ContentType.data(),ContentType.size());
        written=ContentType.size();
        return WebResult::Success();
    }
    HttpReadResult ReadBody(std::uint8_t* destination,std::size_t capacity) override {
        const auto remaining=Body.size()-Offset;
        const auto count=remaining<capacity?remaining:capacity;
        if(count){ std::memcpy(destination,Body.data()+Offset,count);Offset+=count; }
        return {WebResult::Success(),count,Offset==Body.size()};
    }
};

class Response final : public IHttpResponsePlatform {
public:
    HttpStatus Status=HttpStatus::Ok;
    bool Begun=false;
    bool Completed=false;
    WebResult SetStatus(HttpStatus status) override { Status=status;return WebResult::Success(); }
    WebResult SetHeader(std::string_view,std::string_view) override { return WebResult::Success(); }
    WebResult Begin(std::optional<std::size_t>) override { Begun=true;return WebResult::Success(); }
    WebResult Write(const std::uint8_t*,std::size_t) override { return WebResult::Success(); }
    WebResult Complete() override { Completed=true;return WebResult::Success(); }
    void Abort() noexcept override {}
};

class Selector final : public IHttpCommandTargetSelector {
public:
    std::string_view Name=FireAndForgetName;
    std::string_view SelectCommandType(const HttpRequest&,const RouteParameters&) const noexcept override { return Name; }
};

class Authorizer final : public IHttpCommandAuthorizer {
public:
    HttpCommandAuthorizationDecision Decision=HttpCommandAuthorizationDecision::Authorized;
    HttpCommandAuthorizationDecision Authorize(const HttpRequest&,const Primitive::PrimitiveTypeDescriptor&) const noexcept override { return Decision; }
};

using Ingress=HttpCommandIngress<32>;

struct Fixture final {
    Primitive::TypeDirectory<2> Directory;
    Selector Target;
    Authorizer Authorization;
    Ingress Handler;

    Fixture(){
        assert(Directory.Register(Common(Command::CommandTypeId{71},FireAndForgetName,FireAndForgetExtension))==Primitive::TypeDirectoryRegistrationStatus::Success);
        assert(Directory.Register(Common(Command::CommandTypeId{72},RequesterName,RequesterExtension))==Primitive::TypeDirectoryRegistrationStatus::Success);
        assert(Directory.Initialize()==Primitive::TypeDirectoryInitializationStatus::Success);
        HttpCommandIngressConfiguration configuration{};
        configuration.Types=Directory.View();
        configuration.TargetSelector=&Target;
        configuration.Authorizer=&Authorization;
        configuration.ReadChunkBytes=3;
        assert(Handler.Configure(configuration));
    }
};

HttpHandlerResult Invoke(Ingress& ingress,Request& request,Response& response){
    WebRequestContext context(request,response);
    RouteParameters parameters;
    return ingress.Handle(context,parameters);
}

void ResetSubmission(Command::CommandSubmissionStatus status=Command::CommandSubmissionStatus::Accepted){
    NextSubmissionStatus=status;LastPayloadBytes=0;SubmissionCount=0;LastPayload.fill(0);
}

void TestConfigurationRequiresFrozenDiscoveryAndSecurity(){
    Ingress ingress;
    HttpCommandIngressConfiguration configuration{};
    assert(!ingress.Configure(configuration));

    Primitive::TypeDirectory<1> unfrozen;
    Selector selector;
    Authorizer authorizer;
    configuration.Types=unfrozen.View();
    configuration.TargetSelector=&selector;
    configuration.Authorizer=&authorizer;
    assert(!ingress.Configure(configuration));
}

void TestJsonSubmissionUsesTypedDescriptor(){
    Fixture fixture;
    ResetSubmission();
    Request request;
    request.ContentType="application/json; charset=utf-8";
    constexpr std::string_view body="{\"v\":7}";
    request.Body.assign(body.begin(),body.end());
    request.DeclaredLength=request.Body.size();
    Response response;
    const auto result=Invoke(fixture.Handler,request,response);
    assert(result && result.Disposition==HttpHandlerDisposition::Handled);
    assert(response.Status==HttpStatus::Accepted && response.Begun && response.Completed);
    assert(SubmissionCount==1 && LastFormat==Command::CommandPayloadFormat::JSON);
    assert(LastPayloadBytes==body.size());
    assert(std::memcmp(LastPayload.data(),body.data(),body.size())==0);
}

void TestUnknownLengthCborIsBoundedAndSubmitted(){
    Fixture fixture;
    ResetSubmission();
    Request request;
    request.ContentType="application/cbor";
    request.Body={0xa1,0x61,0x76,0x07};
    request.DeclaredLength.reset();
    Response response;
    const auto result=Invoke(fixture.Handler,request,response);
    assert(result && response.Status==HttpStatus::Accepted);
    assert(request.Offset==request.Body.size());
    assert(SubmissionCount==1 && LastFormat==Command::CommandPayloadFormat::CBOR);
}

void TestTargetAuthorizationAndRequesterBoundaries(){
    Fixture fixture;
    ResetSubmission();

    Request request;
    request.ContentType="application/json";
    request.Body={'{','}'};
    request.DeclaredLength=request.Body.size();

    fixture.Target.Name="Missing.Command";
    Response missing;
    assert(Invoke(fixture.Handler,request,missing));
    assert(missing.Status==HttpStatus::NotFound && SubmissionCount==0);

    fixture.Target.Name=FireAndForgetName;
    fixture.Authorization.Decision=HttpCommandAuthorizationDecision::Unauthorized;
    request.Offset=0;Response unauthorized;
    assert(Invoke(fixture.Handler,request,unauthorized));
    assert(unauthorized.Status==HttpStatus::Unauthorized && SubmissionCount==0);

    fixture.Authorization.Decision=HttpCommandAuthorizationDecision::Forbidden;
    request.Offset=0;Response forbidden;
    assert(Invoke(fixture.Handler,request,forbidden));
    assert(forbidden.Status==HttpStatus::Forbidden && SubmissionCount==0);

    fixture.Authorization.Decision=HttpCommandAuthorizationDecision::Authorized;
    fixture.Target.Name=RequesterName;
    request.Offset=0;Response requester;
    assert(Invoke(fixture.Handler,request,requester));
    assert(requester.Status==HttpStatus::Conflict && SubmissionCount==0);
}

void TestMediaAndSchemaBounds(){
    Fixture fixture;
    ResetSubmission();

    Request unsupported;
    unsupported.ContentType="text/plain";
    unsupported.Body={'x'};unsupported.DeclaredLength=1;
    Response unsupportedResponse;
    assert(Invoke(fixture.Handler,unsupported,unsupportedResponse));
    assert(unsupportedResponse.Status==HttpStatus::UnsupportedMediaType && SubmissionCount==0);

    Request knownOversized;
    knownOversized.ContentType="application/json";
    knownOversized.Body.resize(RequestSchema.MaximumJsonBytes+1,'x');
    knownOversized.DeclaredLength=knownOversized.Body.size();
    Response knownResponse;
    assert(Invoke(fixture.Handler,knownOversized,knownResponse));
    assert(knownResponse.Status==HttpStatus::PayloadTooLarge && SubmissionCount==0);

    Request unknownOversized;
    unknownOversized.ContentType="application/json";
    unknownOversized.Body.resize(RequestSchema.MaximumJsonBytes+1,'x');
    unknownOversized.DeclaredLength.reset();
    Response unknownResponse;
    assert(Invoke(fixture.Handler,unknownOversized,unknownResponse));
    assert(unknownResponse.Status==HttpStatus::PayloadTooLarge && SubmissionCount==0);
}

void TestSubmissionStatusMappingAndMethodFallthrough(){
    Fixture fixture;
    Request request;
    request.ContentType="application/octet-stream";
    request.Body={1,2,3};request.DeclaredLength=request.Body.size();

    ResetSubmission(Command::CommandSubmissionStatus::SchemaOrDecodeFailure);
    Response decode;
    assert(Invoke(fixture.Handler,request,decode));
    assert(decode.Status==HttpStatus::UnprocessableContent);

    request.Offset=0;ResetSubmission(Command::CommandSubmissionStatus::CapacityUnavailable);
    Response capacity;
    assert(Invoke(fixture.Handler,request,capacity));
    assert(capacity.Status==HttpStatus::TooManyRequests);

    request.Offset=0;request.MethodValue=HttpMethod::Get;ResetSubmission();
    Response get;
    const auto result=Invoke(fixture.Handler,request,get);
    assert(result && result.Disposition==HttpHandlerDisposition::NotHandled);
    assert(SubmissionCount==0);
}

} // namespace

int main(){
    TestConfigurationRequiresFrozenDiscoveryAndSecurity();
    TestJsonSubmissionUsesTypedDescriptor();
    TestUnknownLengthCborIsBoundedAndSubmitted();
    TestTargetAuthorizationAndRequesterBoundaries();
    TestMediaAndSchemaBounds();
    TestSubmissionStatusMappingAndMethodFallthrough();
    return 0;
}
