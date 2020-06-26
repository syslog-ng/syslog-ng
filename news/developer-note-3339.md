`plugin_skeleton_creator`: fixing a compiler switch

Wrong compiler switch used in `plugin_skeleton_creator`. This caused a compiler warning. The grammar debug info did not appear for that module, when `-y` command line option was used.
