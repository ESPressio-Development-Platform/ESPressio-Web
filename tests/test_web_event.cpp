#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string_view>
#include <vector>

#include <ESPressio_WebEvent.hpp>

using namespace ESPressio;
using namespace ESPressio::Web;

namespace {

constexpr std::string_view EventName="Test.Web.Event";
constexpr std::string_view LocalName="Test.Web.LocalEvent";

const Serializable::StaticSchemaDescriptor EventSchema{
    1,1,1,0,nullptr,0x4321u,8,12,24
};

Event::EventDynamicDispatchStatus NextStatus=Event::EventDynamicDispatchStatus::Accepted;
Event::EventPayloadFormat LastFormat=Event::EventPayloadFormat::DirectBinary;
std::array<std::uint8_t,64> LastPayload{};
std::size_t LastPayloadBytes=0;
std::size_t DispatchCount=0;

Event::EventDynamicDispatchResult Dispatch(
    Event::EventPayloadFormat format,
    const std::uint8_t* payload,
    std::size_t size) noexcept {
    ++DispatchCount;
    LastFormat=format;
    LastPayloadBytes=size;
    if(size<=LastPayload.size() && payload) std::memcpy(LastPayload.data(),payload,size);
    return {NextStatus,Primitive::ConceptualMessageId{41}};
}

const Event::EventTypeDescriptor SerializableExtension=[] {
    Event::EventTypeDescriptor value{};
    value.TypeId=Event::EventTypeId{81};
    value.Tier=Event::EventTier::Serializable;
    value.Schema=&EventSchema;
    value.DynamicallyConstructible=true;
    value.DispatchSerialized=&Dispatch;
    return value;
}();

const Event::EventTypeDescriptor LocalExtension=[] {
    Event::EventTypeDescriptor value{};
    value.TypeId=Event::EventTypeId{82};
    value.Tier=Event::EventTier::Local;
    return value;
}();

Primitive::PrimitiveTypeDescriptor Common(
    Event::EventTypeId type,
    std::string_view name,
    const Event::EventTypeDescriptor& extension) {
    return {
        {Event::EventFamilyId,type.Value()}, name,
        Primitive::PrimitiveTypeCapabilities{1}, {1,1}, {}, {24}, {&extension}
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
    std::string_view Path() const noexcept override { return "/application-owned-event-route"; }
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
        if(count){std::memcpy(destination,Body.data()+Offset,count);Offset+=count;}
        return {WebResult::Success(),count,Offset==Body.size()};
    }
};

class Response final : public IHttpResponsePlatform {
public:
    HttpStatus Status=HttpStatus::Ok;
    bool Begun=false,Completed=false;
    WebResult SetStatus(HttpStatus status) override {Status=status;return WebResult::Success();}
    WebResult SetHeader(std::string_view,std::string_view) override {return WebResult::Success();}
    WebResult Begin(std::optional<std::size_t>) override {Begun=true;return WebResult::Success();}
    WebResult Write(const std::uint8_t*,std::size_t) override {return WebResult::Success();}
    WebResult Complete() override {Completed=true;return WebResult::Success();}
    void Abort() noexcept override {}
};

class Selector final : public IHttpEventTargetSelector {
public:
    std::string_view Name=EventName;
    std::string_view SelectEventType(const HttpRequest&,const RouteParameters&) const noexcept override {return Name;}
};

class Authorizer final : public IHttpEventAuthorizer {
public:
    HttpEventAuthorizationDecision Decision=HttpEventAuthorizationDecision::Authorized;
    HttpEventAuthorizationDecision Authorize(const HttpRequest&,const Primitive::PrimitiveTypeDescriptor&) const noexcept override {return Decision;}
};

using Ingress=HttpEventIngress<32>;
struct Fixture final {
    Primitive::TypeDirectory<2> Directory;
    Selector Target;
    Authorizer Authorization;
    Ingress Handler;
    Fixture(){
        assert(Directory.Register(Common(Event::EventTypeId{81},EventName,SerializableExtension))==Primitive::TypeDirectoryRegistrationStatus::Success);
        assert(Directory.Register(Common(Event::EventTypeId{82},LocalName,LocalExtension))==Primitive::TypeDirectoryRegistrationStatus::Success);
        assert(Directory.Initialize()==Primitive::TypeDirectoryInitializationStatus::Success);
        HttpEventIngressConfiguration configuration{};
        configuration.Types=Directory.View();configuration.TargetSelector=&Target;
        configuration.Authorizer=&Authorization;configuration.ReadChunkBytes=3;
        assert(Handler.Configure(configuration));
    }
};

HttpHandlerResult Invoke(Ingress& ingress,Request& request,Response& response){
    WebRequestContext context(request,response);RouteParameters parameters;
    return ingress.Handle(context,parameters);
}
void Reset(Event::EventDynamicDispatchStatus status=Event::EventDynamicDispatchStatus::Accepted){
    NextStatus=status;DispatchCount=0;LastPayloadBytes=0;LastPayload.fill(0);
}

void TestConfigurationRequiresFrozenDirectoryAndSecurity(){
    Ingress ingress;HttpEventIngressConfiguration configuration{};
    assert(!ingress.Configure(configuration));
    Primitive::TypeDirectory<1> unfrozen;Selector selector;Authorizer authorizer;
    configuration.Types=unfrozen.View();configuration.TargetSelector=&selector;configuration.Authorizer=&authorizer;
    assert(!ingress.Configure(configuration));
}

void TestDescriptorDrivenJsonDispatch(){
    Fixture fixture;Reset();Request request;
    request.ContentType="application/json; charset=utf-8";
    constexpr std::string_view body="{\"v\":9}";
    request.Body.assign(body.begin(),body.end());request.DeclaredLength=request.Body.size();
    Response response;const auto result=Invoke(fixture.Handler,request,response);
    assert(result && result.Disposition==HttpHandlerDisposition::Handled);
    assert(response.Status==HttpStatus::Accepted && response.Begun && response.Completed);
    assert(DispatchCount==1 && LastFormat==Event::EventPayloadFormat::JSON);
    assert(LastPayloadBytes==body.size() && std::memcmp(LastPayload.data(),body.data(),body.size())==0);
}

void TestUnknownLengthCborIsBounded(){
    Fixture fixture;Reset();Request request;
    request.ContentType="application/cbor";request.Body={0xa1,0x61,0x76,0x09};request.DeclaredLength.reset();
    Response response;assert(Invoke(fixture.Handler,request,response));
    assert(response.Status==HttpStatus::Accepted && DispatchCount==1 && LastFormat==Event::EventPayloadFormat::CBOR);
}

void TestDiscoveryAuthorizationAndConstructibility(){
    Fixture fixture;Reset();Request request;
    request.ContentType="application/json";request.Body={'{','}'};request.DeclaredLength=2;
    fixture.Target.Name="Missing.Event";Response missing;assert(Invoke(fixture.Handler,request,missing));
    assert(missing.Status==HttpStatus::NotFound && DispatchCount==0);
    fixture.Target.Name=EventName;fixture.Authorization.Decision=HttpEventAuthorizationDecision::Unauthorized;
    request.Offset=0;Response unauthorized;assert(Invoke(fixture.Handler,request,unauthorized));
    assert(unauthorized.Status==HttpStatus::Unauthorized && DispatchCount==0);
    fixture.Authorization.Decision=HttpEventAuthorizationDecision::Forbidden;
    request.Offset=0;Response forbidden;assert(Invoke(fixture.Handler,request,forbidden));
    assert(forbidden.Status==HttpStatus::Forbidden && DispatchCount==0);
    fixture.Authorization.Decision=HttpEventAuthorizationDecision::Authorized;fixture.Target.Name=LocalName;
    request.Offset=0;Response local;assert(Invoke(fixture.Handler,request,local));
    assert(local.Status==HttpStatus::UnprocessableContent && DispatchCount==0);
}

void TestMediaBoundsStatusAndMethod(){
    Fixture fixture;Reset();Request request;
    request.ContentType="text/plain";request.Body={'x'};request.DeclaredLength=1;
    Response media;assert(Invoke(fixture.Handler,request,media));assert(media.Status==HttpStatus::UnsupportedMediaType);
    request.Offset=0;request.ContentType="application/json";request.Body.resize(EventSchema.MaximumJsonBytes+1,'x');request.DeclaredLength=request.Body.size();
    Response large;assert(Invoke(fixture.Handler,request,large));assert(large.Status==HttpStatus::PayloadTooLarge && DispatchCount==0);
    request.Offset=0;request.Body={1};request.DeclaredLength=1;request.ContentType="application/octet-stream";Reset(Event::EventDynamicDispatchStatus::CapacityUnavailable);
    Response capacity;assert(Invoke(fixture.Handler,request,capacity));assert(capacity.Status==HttpStatus::TooManyRequests);
    request.Offset=0;Reset(Event::EventDynamicDispatchStatus::SchemaOrDecodeFailure);Response decode;
    assert(Invoke(fixture.Handler,request,decode));assert(decode.Status==HttpStatus::UnprocessableContent);
    request.MethodValue=HttpMethod::Get;request.Offset=0;Reset();Response get;const auto result=Invoke(fixture.Handler,request,get);
    assert(result && result.Disposition==HttpHandlerDisposition::NotHandled && DispatchCount==0);
}

} // namespace

int main(){
    TestConfigurationRequiresFrozenDirectoryAndSecurity();
    TestDescriptorDrivenJsonDispatch();
    TestUnknownLengthCborIsBounded();
    TestDiscoveryAuthorizationAndConstructibility();
    TestMediaBoundsStatusAndMethod();
    return 0;
}
