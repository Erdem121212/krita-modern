set (krita_BUNDLE_FILES
     README
     Krita_4_Default_Resources.bundle
     Krita_Artists_SeExpr_examples.bundle
     RGBA_brushes.bundle
     Kgaia_brushes_1.bundle
     Rakurri_Brush_Set_V2.0.bundle
     stupid_idiot_bundle.bundle
)

if (NOT ANDROID)
    set (krita_BUNDLE_FILES
         ${krita_BUNDLE_FILES}
         Krita_3_Default_Resources.bundle
         )
endif()

install( FILES
    ${krita_BUNDLE_FILES}
    DESTINATION  ${KDE_INSTALL_DATADIR}/krita/bundles)
