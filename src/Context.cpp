#include <SFGUI/Context.hpp>
#include <SFGUI/RenderEngine.hpp>
#include <SFGUI/Engines/BREW.hpp>
#include <boost/scoped_ptr.hpp>

namespace sfg {

Context*  Context::active_context( 0 );

Context& Context::Get() {
	if( active_context ) {
		return *active_context;
	}

	static eng::BREW  brew;
	static Context  context( brew );

	return context;
}

bool Context::Activate( Context& context ) {
	if( active_context ) {
		return false;
	}

	active_context = &context;
	return true;
}

bool Context::Deactivate() {
	if( !active_context ) {
		return false;
	}

	active_context = 0;
	return true;
}

Context::Context( RenderEngine& engine ) :
	m_renderengine( engine )
{
}

RenderEngine& Context::GetRenderEngine() const {
	return m_renderengine;
}

}