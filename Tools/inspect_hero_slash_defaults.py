import unreal


def log(message):
    unreal.log_warning("[HeroSlashDiag] " + message)


def read_property(obj, name):
    try:
        return obj.get_editor_property(name)
    except Exception as exc:
        return "<unavailable: {}>".format(exc)


def inspect_projectile(asset_path):
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not asset:
        log("FAILED to load {}".format(asset_path))
        return

    generated_class = asset.generated_class()
    defaults = unreal.get_default_object(generated_class)
    log("asset={} class={}".format(
        asset_path,
        generated_class.get_name(),
    ))
    log("initial_life_span={} spawn_collision_handling_method={} projectile_damage_policy={}".format(
        read_property(defaults, "initial_life_span"),
        read_property(defaults, "spawn_collision_handling_method"),
        read_property(defaults, "projectile_damage_policy"),
    ))

    components = defaults.get_components_by_class(unreal.ActorComponent)
    for component in components:
        class_name = component.get_class().get_name()
        log("component={} class={}".format(component.get_name(), class_name))

        if isinstance(component, unreal.BoxComponent):
            log("  box_extent={} relative_location={} collision_enabled={} profile={} pawn_response={}".format(
                component.get_unscaled_box_extent(),
                read_property(component, "relative_location"),
                component.get_collision_enabled(),
                component.get_collision_profile_name(),
                component.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN),
            ))

        if isinstance(component, unreal.ProjectileMovementComponent):
            log("  initial_speed={} max_speed={} velocity={} gravity={} rotation_follows_velocity={} active={}".format(
                read_property(component, "initial_speed"),
                read_property(component, "max_speed"),
                read_property(component, "velocity"),
                read_property(component, "projectile_gravity_scale"),
                read_property(component, "rotation_follows_velocity"),
                component.is_active(),
            ))


def inspect_axe_socket():
    mesh = unreal.EditorAssetLibrary.load_asset("/Game/Assets/Weapons/Axe/SM_DefaultAxe")
    if not mesh:
        log("FAILED to load axe mesh")
        return

    sockets = read_property(mesh, "sockets")
    log("axe sockets={}".format(sockets))
    if isinstance(sockets, list):
        for socket in sockets:
            log("  socket={} relative_location={} relative_rotation={} relative_scale={}".format(
                read_property(socket, "socket_name"),
                read_property(socket, "relative_location"),
                read_property(socket, "relative_rotation"),
                read_property(socket, "relative_scale"),
            ))

    component = unreal.StaticMeshComponent()
    component.set_static_mesh(mesh)
    for socket_name in ("RageSlashSocket", "RageSlashSocketNone"):
        log("axe socket query name={} exists={} component_transform={}".format(
            socket_name,
            component.does_socket_exist(socket_name),
            component.get_socket_transform(
                socket_name,
                unreal.RelativeTransformSpace.RTS_COMPONENT,
            ),
        ))


inspect_projectile("/Game/PlayerCharacter/BP_Projectile_HeroSlash")
inspect_projectile("/Game/EnemyCharacter/Gruntling/Glacer/BP_Projectile_Glacer")
inspect_axe_socket()
