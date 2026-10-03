#pragma once

#include <mods/service.hpp>

#if __has_include(<mods/svc/actor_attribute.h>)
#include <mods/svc/actor_attribute.h>
#else
#define ACTOR_ATTRIBUTE_SERVICE_ID "dev.twilitrealm.dusklight.actor_attribute"
#define ACTOR_ATTRIBUTE_SERVICE_MAJOR 1u
#define ACTOR_ATTRIBUTE_SERVICE_MINOR 5u

typedef uint64_t ActorAttributeResolverHandle;

typedef enum ActorAttribute {
    ACTOR_ATTRIBUTE_MOVEMENT_SPEED = 0,
    ACTOR_ATTRIBUTE_SIZE = 1,
    ACTOR_ATTRIBUTE_HEALTH = 2,
    ACTOR_ATTRIBUTE_ATTACK_DAMAGE = 3,
    ACTOR_ATTRIBUTE_GRAVITY = 4,
    ACTOR_ATTRIBUTE_NOTICE_RANGE = 5,
    ACTOR_ATTRIBUTE_PLAYER_KNOCKBACK = 6,
    ACTOR_ATTRIBUTE_STUN_DURATION = 7,
} ActorAttribute;

typedef struct ActorAttributeInfo {
    const void* actor;
    ActorAttribute attribute;
    float vanilla_value;
    float current_value;
} ActorAttributeInfo;

typedef bool (*ActorAttributeResolveFn)(ModContext* ctx, const ActorAttributeInfo* info,
    float* out_value, void* user_data);

typedef struct ActorAttributeService {
    ServiceHeader header;

    ModResult (*register_resolver)(ModContext* ctx, ActorAttributeResolveFn fn, void* user_data,
        ActorAttributeResolverHandle* out_handle);

    ModResult (*unregister_resolver)(ModContext* ctx, ActorAttributeResolverHandle handle);

    float (*resolve)(const void* actor, ActorAttribute attribute, float vanilla_value);
} ActorAttributeService;

MOD_DECLARE_SERVICE(ActorAttributeService, svc_actor_attribute, ACTOR_ATTRIBUTE_SERVICE_ID,
    ACTOR_ATTRIBUTE_SERVICE_MAJOR, ACTOR_ATTRIBUTE_SERVICE_MINOR);
#endif
