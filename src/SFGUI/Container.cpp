#include <SFGUI/Container.hpp>

namespace sfg {

Container::Container() :
	Widget(),
	m_border_width( 0.f ),
	m_children_size( 0 )
{
}

Container::~Container() {
}

void Container::Add( const Widget::Ptr& widget ) {
	if( IsChild( widget ) ) {
		return;
	}

	m_children.push_back( widget );
	++m_children_size;
	HandleAdd( widget );

	// Check if HandleAdd still wants the little boy.
	if( IsChild( widget ) ) {
		widget->SetParent( shared_from_this() );
		RequestResize();
	}
}

void Container::Remove( const Widget::Ptr& widget ) {
	WidgetsList::iterator iter( std::find( m_children.begin(), m_children.end(), widget ) );

	if( iter != m_children.end() ) {
		m_children.erase( iter );
		--m_children_size;
		RequestResize();

		HandleRemove( widget );
	}
}

bool Container::IsChild( const Widget::Ptr& widget ) const {
	for( std::size_t index = 0; index < m_children_size; ++index ) {
		if( m_children[index] == widget ) {
			return true;
		}
	}

	return false;
}

const Container::WidgetsList& Container::GetChildren() const {
	return m_children;
}

void Container::HandleExpose( CullingTarget& target ) const {
	for( std::size_t index = 0; index < m_children_size; ++index ) {
		m_children[index]->Expose( target );
	}
}

void Container::SetBorderWidth( float width ) {
	m_border_width = width;
	RequestResize();
	Invalidate();
}

float Container::GetBorderWidth() const {
	return m_border_width;
}

void Container::Refresh() {
	for( std::size_t index = 0; index < m_children_size; ++index ) {
		m_children[index]->Refresh();
	}

	Widget::Refresh();
}

void Container::HandleEvent( const sf::Event& event ) {
	// Ignore event when widget is not visible.
	if( !IsVisible() ) {
		return;
	}

	// Create a copy of the event and transform mouse coordinates to local
	// coordinates if event is a mouse event.
	sf::Event local_event( event );

	if( local_event.Type == sf::Event::MouseMoved ) {
		local_event.MouseMove.X -= static_cast<int>( GetAllocation().Left );
		local_event.MouseMove.Y -= static_cast<int>( GetAllocation().Top );
	}

	if(
		local_event.Type == sf::Event::MouseButtonPressed ||
		local_event.Type == sf::Event::MouseButtonReleased
	) {
		local_event.MouseButton.X -= static_cast<int>( GetAllocation().Left );
		local_event.MouseButton.Y -= static_cast<int>( GetAllocation().Top );
	}

	// Pass event to children.
	for( std::size_t index = 0; index < m_children_size; ++index ) {
		m_children[index]->HandleEvent( local_event );
	}

	// Process event for own widget.
	Widget::HandleEvent( event );
}

void Container::HandleAdd( const Widget::Ptr& /*child*/ ) {
}

void Container::HandleRemove( const Widget::Ptr& /*child*/ ) {
}

void Container::HandleChildInvalidate( const Widget::PtrConst& child ) const {
	Container::PtrConst parent = GetParent();

	if( parent ) {
		parent->HandleChildInvalidate( child );
	}
}

void Container::HandleAbsolutePositionChange() {
	// Update children's drawable positions.
	for( std::size_t index = 0; index < m_children_size; ++index ) {
		m_children[index]->HandleAbsolutePositionChange();
	}

	// Update own drawable position.
	UpdateDrawablePosition();
}

}
