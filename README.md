# ngspice v31 macOS installer with patch for Autodesk EAGLE

This ngspice v31 installer includes a signed mac OS installer package called ngspice.pkg. The installer package is generated using the script entitled build-for-mac-os.sh. If you don't trust the installer, review the build script, change the prefix and exec-prefix directories to the absolute directory where you downloaded the source from GitHub, then build it for yourself. The pre and post install scripts for the installer package are in the scripts directory. You will have to supply your own signing key if you wish to distribute, or install on macOS 10.12 and newer.

The installer package has a patch for EAGLE, it will locate EAGLE installations and will install ngspice v31 into the EAGLE installation directories.

EAGLE has included ngspice v26 for a number of major releases now, and I have experienced issues with simulation. Updating to ngspice v31 fixed these issues.

As it helped me, I thought it would be courteous to offer the fix to others.

This fix is experimental, and it creates a backup copy of the Autodesk included ngspice directory in the EAGLE installation directory under ngspice-old.

If at any time you want to revert to the Autodesk included ngspice, just delete the ngspice directory in the EAGLE installation directory (e.g. /Applications/EAGLE-9.6.0/), and then rename the 'ngspice-old' directory to 'ngspice' 
