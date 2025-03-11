import bpy
def clear_all_custom_data_properties():
        for data in bpy.data.objects:
            data.id_properties_clear()
clear_all_custom_data_properties()