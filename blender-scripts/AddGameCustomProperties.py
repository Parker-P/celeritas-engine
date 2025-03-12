import bpy

objects = bpy.context.scene.objects

for obj in objects:
    if obj.type == 'MESH':
        if "EnableGravity" not in obj:
            obj["EnableGravity"] = True
            rna_prop = obj.id_properties_ui("EnableGravity")
            rna_prop.update(
                default=True,
                description="Toggles gravity effect on this object"
            )
        
        if "CollisionMeshName" not in obj:
            obj["CollisionMeshName"] = obj.name
            rna_prop = obj.id_properties_ui("CollisionMeshName")
            rna_prop.update(
                default=obj.name,
                description="Name of the mesh used for collision detection"
            )
        
        if "Friction" not in obj:
            obj["Friction"] = 20.0
            rna_prop = obj.id_properties_ui("Friction")
            rna_prop.update(
                default=20.0,
                min=0.0,
                max=100.0,
                description="Surface friction coefficient (0-100)"
            )
        
        if "Mass" not in obj:
            obj["Mass"] = 1.0
            rna_prop = obj.id_properties_ui("Mass")
            rna_prop.update(
                default=1.0,
                min=0.0,
                description="Mass of the object in kilograms"
            )
        
        if "LockRotationX" not in obj:
            obj["LockRotationX"] = False
            rna_prop = obj.id_properties_ui("LockRotationX")
            rna_prop.update(
                default=False,
                description="Blender -Y = Engine +Z, Blender -X = Engine +X, Blender +Z = Engine +Y"
            )
        
        if "LockRotationY" not in obj:
            obj["LockRotationY"] = False
            rna_prop = obj.id_properties_ui("LockRotationY")
            rna_prop.update(
                default=False,
                description="Blender -Y = Engine +Z, Blender -X = Engine +X, Blender +Z = Engine +Y"
            )
        
        if "LockRotationZ" not in obj:
            obj["LockRotationZ"] = False
            rna_prop = obj.id_properties_ui("LockRotationZ")
            rna_prop.update(
                default=False,
                description="Blender -Y = Engine +Z, Blender -X = Engine +X, Blender +Z = Engine +Y"
            )
        
        if "LockTranslationX" not in obj:
            obj["LockTranslationX"] = False
            rna_prop = obj.id_properties_ui("LockTranslationX")
            rna_prop.update(
                default=False,
                description="Blender -Y = Engine +Z, Blender -X = Engine +X, Blender +Z = Engine +Y"
            )
        
        if "LockTranslationY" not in obj:
            obj["LockTranslationY"] = False
            rna_prop = obj.id_properties_ui("LockTranslationY")
            rna_prop.update(
                default=False,
                description="Blender -Y = Engine +Z, Blender -X = Engine +X, Blender +Z = Engine +Y"
            )
        
        if "LockTranslationZ" not in obj:
            obj["LockTranslationZ"] = False
            rna_prop = obj.id_properties_ui("LockTranslationZ")
            rna_prop.update(
                default=False,
                description="Blender -Y = Engine +Z, Blender -X = Engine +X, Blender +Z = Engine +Y"
            )
        
        if "IsCollidable" not in obj:
            obj["IsCollidable"] = True
            rna_prop = obj.id_properties_ui("IsCollidable")
            rna_prop.update(
                default=True,
                description="Determines if this object participates in collision detection"
            )