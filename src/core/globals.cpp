#include "core/plugin_system.hpp"
#include "core/advanced_template_engine.hpp"
#include <memory>

namespace crossexpand {

// Global instances for Day 3 integration
std::unique_ptr<PluginManager> g_plugin_manager;
std::unique_ptr<AdvancedTemplateEngine> g_advanced_template_engine;

} // namespace crossexpand
