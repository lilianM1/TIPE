import os
from SCons.Script import DefaultEnvironment

def post_upload(source, target, env):
    print("Upload terminé, commit et push en cours...")
    # Commandes Git
    os.system("git add .")
    os.system('git commit -m "Auto-commit après upload PlatformIO"')
    os.system("git push")
    print("Commit et push terminés !")

# Associe la fonction à l'événement "post:upload"
DefaultEnvironment().AddPostAction("upload", post_upload)