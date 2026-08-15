local boundary = require("core:module.boundary.entrypoints")
local resources = require("core:module.resources.service")
local state_validator = require("core:module.runtime.state_validator")
local instance_allocator = require("core:module.runtime.instance_allocator")
local state_hasher = require("core:module.runtime.state_hasher")
local mutation_window = require("core:module.runtime.mutation_window")
local actor_registry = require("core:module.runtime.actor_registry")
local world = require("core:module.runtime.world")
local service_registry = require("core:module.runtime.service_registry")
local validator_registry = require("core:module.runtime.validator_registry")
local subscriber_registry = require("core:module.runtime.subscriber_registry")
local event_bus = require("core:module.runtime.event_bus")
local location_service = require("core:module.gameplay.location_service")

return {
    id = "core:module.bootstrap.main",
    boundary = boundary,
    resources = resources,
    state_validator = state_validator,
    instance_allocator = instance_allocator,
    state_hasher = state_hasher,
    mutation_window = mutation_window,
    actor_registry = actor_registry,
    world = world,
    service_registry = service_registry,
    validator_registry = validator_registry,
    subscriber_registry = subscriber_registry,
    event_bus = event_bus,
    location_service = location_service,
}
