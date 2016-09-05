import sys

from src.drivers.file.file_writer import FileWriter


class ConfigWriter(object):
    def __init__(self, config_elements, config_path):
        self.config_elements = config_elements
        self.config_path = config_path
        self.file_writer = FileWriter()

    def write_config(self):
        if self.config_elements["version"]:
            self.render_version()
        else:
            sys.exit(1)
        if self.config_elements["global_options"]:
            self.render_global_options()
        if self.config_elements["source_groups"]:
            self.render_source_drivers()
        else:
            sys.exit(1)
        if self.config_elements["parser_groups"]:
            pass
        if self.config_elements["template_groups"]:
            pass
        if self.config_elements["rewrite_groups"]:
            pass
        if self.config_elements["filter_groups"]:
            pass
        if self.config_elements["destination_groups"]:
            self.render_destination_drivers()
        else:
            sys.exit(1)
        if self.config_elements["logpaths"]:
            self.render_logpath()
        else:
            sys.exit(1)

    def render_version(self):
        self.file_writer.create_file_with_content(self.config_path, "@version: %s" % self.config_elements["version"])

    def render_global_options(self):
        self.file_writer.append_file_with_content(self.config_path, "options {")
        for option_name, option_value in self.config_elements["global_options"].items():
            self.file_writer.append_file_with_content(self.config_path, "    %s(%s);" % (option_name, option_value))
        self.file_writer.append_file_with_content(self.config_path, "};")

    def render_source_drivers(self):
        self.render_drivers(group_type="source")

    def render_destination_drivers(self):
        self.render_drivers(group_type="destination")

    def render_drivers(self, group_type):
        for group in self.config_elements["%s_groups" % group_type]:
            # Note: Render group header
            self.file_writer.append_file_with_content(self.config_path, "%s %s {" % (group_type, group["group_name"]))
            for driver in group["drivers"]:
                # Note: Render driver header
                self.file_writer.append_file_with_content(self.config_path, "    %s (" % driver["driver_name"])
                if driver["driver_options"] is not None:
                    if ("ip" in driver["driver_options"].keys()) and (group_type == "destination"):
                        # We should write ip as first option
                        for option_name, option_value in driver["driver_options"].items():
                            if (option_name == "ip"):
                                self.file_writer.append_file_with_content(self.config_path, "        %s" % option_value)

                    for option_name, option_value in driver["driver_options"].items():
                        # Note: Render driver options
                        if option_name == "file_path":
                            self.file_writer.append_file_with_content(self.config_path, "        %s" % option_value)
                        elif ("ip" in driver["driver_options"].keys()) and (group_type == "destination"):
                            # Note already processed
                            pass
                        else:
                            self.file_writer.append_file_with_content(self.config_path,
                                                                      "        %s(%s)" % (option_name, option_value))
                # Note: Render driver footer
                self.file_writer.append_file_with_content(self.config_path, "    );")
            # Note: Render group footer
            self.file_writer.append_file_with_content(self.config_path, "};")

    def render_logpath(self):
        for logpath in self.config_elements["logpaths"]:
            self.file_writer.append_file_with_content(self.config_path, "log {")
            for src_driver in logpath["source_drivers"]:
                self.file_writer.append_file_with_content(self.config_path, "    source(%s);" % src_driver)
            for dst_driver in logpath["destination_drivers"]:
                self.file_writer.append_file_with_content(self.config_path, "    destination(%s);" % dst_driver)
            self.file_writer.append_file_with_content(self.config_path, "};")
