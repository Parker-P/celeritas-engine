import bpy
objects = bpy.context.scene.objects
for obj in objects:
    if obj.type == 'MESH':
        if "EnableGravityP" not in obj:
            obj["EnableGravityP"] = True
        if "CollisionMeshNameP" not in obj:
            obj["CollisionMeshNameP"] = obj.name
        if "FrictionP" not in obj:
            obj["FrictionP"] = 20.0