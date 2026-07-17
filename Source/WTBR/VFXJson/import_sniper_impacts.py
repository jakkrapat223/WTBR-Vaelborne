import unreal


COMMANDS = [
    r'WTBR.Niagara.ImportJson E:\World Trigger\WTBR-Vaelborne\Source\WTBR\VFXJson\NS_Telorn_Impact.json',
    r'WTBR.Niagara.ImportJson E:\World Trigger\WTBR-Vaelborne\Source\WTBR\VFXJson\NS_Piercex_Impact.json',
    r'WTBR.Niagara.ImportJson E:\World Trigger\WTBR-Vaelborne\Source\WTBR\VFXJson\NS_Fulgris_Impact.json',
    'WTBR.Niagara.DumpUserParams /Game/VFX/Sniper/NS_Telorn_Impact',
    'WTBR.Niagara.DumpUserParams /Game/VFX/Sniper/NS_Piercex_Impact',
    'WTBR.Niagara.DumpUserParams /Game/VFX/Sniper/NS_Fulgris_Impact',
]


world = unreal.EditorLevelLibrary.get_editor_world()

for command in COMMANDS:
    unreal.log('WTBR sniper impact import command: {}'.format(command))
    unreal.SystemLibrary.execute_console_command(world, command)
