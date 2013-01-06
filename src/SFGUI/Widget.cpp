#include <SFGUI/Widget.hpp>
#include <SFGUI/Container.hpp>
#include <SFGUI/Renderer.hpp>
#include <SFGUI/RendererViewport.hpp>
#include <SFGUI/Context.hpp>
#include <cmath>

namespace sfg {

// Signals.
Signal::SignalID Widget::OnStateChange = 0;
Signal::SignalID Widget::OnGainFocus = 0;
Signal::SignalID Widget::OnLostFocus = 0;

Signal::SignalID Widget::OnExpose = 0;

Signal::SignalID Widget::OnSizeAllocate = 0;
Signal::SignalID Widget::OnSizeRequest = 0;

Signal::SignalID Widget::OnMouseEnter = 0;
Signal::SignalID Widget::OnMouseLeave = 0;
Signal::SignalID Widget::OnMouseMove = 0;
Signal::SignalID Widget::OnMouseLeftPress = 0;
Signal::SignalID Widget::OnMouseRightPress = 0;
Signal::SignalID Widget::OnMouseLeftRelease = 0;
Signal::SignalID Widget::OnMouseRightRelease = 0;

Signal::SignalID Widget::OnLeftClick = 0;
Signal::SignalID Widget::OnRightClick = 0;

Signal::SignalID Widget::OnKeyPress = 0;
Signal::SignalID Widget::OnKeyRelease = 0;
Signal::SignalID Widget::OnText = 0;

WeakPtr<Widget> Widget::m_focus_widget;
WeakPtr<Widget> Widget::m_active_widget;
WeakPtr<Widget> Widget::m_modal_widget;

Widget::Widget() :
	Object(),
	m_allocation( 0.f, 0.f, 0.f, 0.f ),
	m_requisition( 0.f, 0.f ),
	m_custom_requisition( 0 ),
	m_class_id( 0 ),
	m_hierarchy_level( 0 ),
	m_z_order( 0 ),
	m_drawable( 0 ),
	m_bitfield( static_cast<unsigned char>( 0xe1 ) ),
	m_invalidated( true )
{
	m_viewport = Renderer::Get().GetDefaultViewport();
}

Widget::~Widget() {
	delete m_drawable;
	delete m_custom_requisition;
	delete m_class_id;
}

bool Widget::IsLocallyVisible() const {
	return ( m_bitfield & 0x01 );
}

bool Widget::IsGloballyVisible() const {
	// If not locally visible, also cannot be globally visible.
	if( !IsLocallyVisible() ) {
		return false;
	}

	// At this point we know the widget is locally visible.

	PtrConst parent( m_parent.lock() );

	// If locally visible and no parent, globally visible.
	if( !parent ) {
		return true;
	}

	// Return parent's global visibility.
	return parent->IsGloballyVisible();
}

void Widget::GrabFocus( Ptr widget ) {
	// Notify old focused widget.
	if( m_focus_widget.lock() ) {
		m_focus_widget.lock()->GetSignals().Emit( OnLostFocus );
		m_focus_widget.lock()->HandleFocusChange( widget );
	}

	m_focus_widget = widget;

	if( m_focus_widget.lock() ) {
		m_focus_widget.lock()->GetSignals().Emit( OnGainFocus );
		m_focus_widget.lock()->HandleFocusChange( widget );
	}
}

bool Widget::HasFocus( PtrConst widget ) {
	if( m_focus_widget.lock() == widget ) {
		return true;
	}

	return false;
}

void Widget::SetAllocation( const sf::FloatRect& rect ) {
	sf::FloatRect oldallocation( m_allocation );

	// Make sure allocation is pixel-aligned.
	m_allocation.left = std::floor( rect.left + .5f );
	m_allocation.top = std::floor( rect.top + .5f );
	m_allocation.width = std::floor( rect.width + .5f );
	m_allocation.height = std::floor( rect.height + .5f );

	if(
		oldallocation.top == m_allocation.top &&
		oldallocation.left == m_allocation.left &&
		oldallocation.width == m_allocation.width &&
		oldallocation.height == m_allocation.height
	) {
		// Nothing even changed. Save the hierarchy the trouble.
		return;
	}

	if( ( oldallocation.top != m_allocation.top ) || ( oldallocation.left != m_allocation.left ) ) {
	  HandlePositionChange();
	  HandleAbsolutePositionChange();
	}

	if( ( oldallocation.width != m_allocation.width ) || ( oldallocation.height != m_allocation.height ) ) {
	  HandleSizeChange();

	  Invalidate();

	  GetSignals().Emit( OnSizeAllocate );
	}
}

void Widget::RequestResize() {
	m_requisition = CalculateRequisition();

	if( m_custom_requisition ) {
		if( m_custom_requisition->x > 0.f ) {
			m_requisition.x = std::max( m_custom_requisition->x, m_requisition.x );
		}

		if( m_custom_requisition->y > 0.f ) {
			m_requisition.y = std::max( m_custom_requisition->y, m_requisition.y );
		}
	}

	HandleRequisitionChange();

	Container::Ptr parent = m_parent.lock();

	// Notify observers.
	GetSignals().Emit( OnSizeRequest );

	if( parent ) {
		parent->RequestResize();
	}
	else {
		sf::FloatRect allocation(
			GetAllocation().left,
			GetAllocation().top,
			std::max( GetAllocation().width, m_requisition.x ),
			std::max( GetAllocation().height, m_requisition.y )
		);

		SetAllocation( allocation );
	}
}


const sf::FloatRect& Widget::GetAllocation() const {
	return m_allocation;
}

void Widget::Update( float seconds ) {
	if( m_invalidated ) {
		m_invalidated = false;

		delete m_drawable;
		m_drawable = InvalidateImpl();

		if( m_drawable ) {
			m_drawable->SetPosition( GetAbsolutePosition() );
			m_drawable->SetLevel( m_hierarchy_level );
			m_drawable->SetZOrder( m_z_order );
			m_drawable->Show( IsGloballyVisible() );

			// We don't want to propagate container viewports for GLCanvases,
			// they have their own special viewport for GL drawing.
			if( !m_drawable->GetPrimitives().empty() && !m_drawable->GetPrimitives()[0]->GetCustomDrawCallback() ) {
				m_drawable->SetViewport( m_viewport );
			}
		}
	}

	HandleUpdate( seconds );
}

void Widget::Invalidate() const {
	if( m_invalidated ) {
		return;
	}

	m_invalidated = true;

	Container::PtrConst parent = m_parent.lock();

	if( parent ) {
		parent->HandleChildInvalidate( static_cast<Widget::PtrConst>( shared_from_this() ) );
	}
}

RenderQueue* Widget::InvalidateImpl() const {
	return 0;
}

void Widget::SetParent( const Widget::Ptr& parent ) {
	Container::Ptr cont( DynamicPointerCast<Container>( parent ) );

	if( !cont ) {
		return;
	}

	Container::Ptr oldparent = m_parent.lock();

	if( oldparent ) {
		oldparent->Remove( shared_from_this() );
	}

	m_parent = cont;

	SetHierarchyLevel( parent->GetHierarchyLevel() + 1 );

	HandleAbsolutePositionChange();
}

void Widget::SetPosition( const sf::Vector2f& position ) {
	sf::FloatRect allocation( GetAllocation() );

	// Make sure allocation is pixel-aligned.
	m_allocation.left = std::floor( position.x + .5f );
	m_allocation.top = std::floor( position.y + .5f );

	if( ( allocation.top != m_allocation.top ) || ( allocation.left != m_allocation.left ) ) {
	  HandlePositionChange();
	  HandleAbsolutePositionChange();
	}
}

void Widget::SetSensitive( bool state ) {
	if( state ) {
		SetState(NORMAL);
	} else {
		SetState(INSENSITIVE);
	}
}

bool Widget::IsSensitive() {
	if( GetState() != INSENSITIVE ) {
		return true;
	} else {
		return false;
	}
}

void Widget::HandleEvent( const sf::Event& event ) {
	if( !IsGloballyVisible() ) {
		return;
	}

	// Ignore the event if widget is insensitive
	if ( GetState() == INSENSITIVE ) {
		return;
	}

	// Ignore the event if another widget is active.
	if( !IsActiveWidget() && !IsActiveWidget( PtrConst() ) ) {
		return;
	}

	// Ignore the event if another widget is modal.
	if( HasModal() && !IsModal() ) {
		return;
	}

	// Set widget active in context.
	Context::Get().SetActiveWidget( shared_from_this() );

	Container::Ptr parent( m_parent.lock() );

	switch( event.type ) {
		case sf::Event::MouseMoved:
			// Check if pointer inside of widget's allocation.
			if( GetAllocation().contains( static_cast<float>( event.mouseMove.x ), static_cast<float>( event.mouseMove.y ) ) ) {
				// Check for enter event.
				if( !IsMouseInWidget() ) {
					// Flip the mouse_in bit.
					m_bitfield ^= static_cast<unsigned char>( 0x10 );

					GetSignals().Emit( OnMouseEnter );
					HandleMouseEnter( event.mouseMove.x, event.mouseMove.y );
				}

				GetSignals().Emit( OnMouseMove );
			}
			else if( IsMouseInWidget() ) { // Check for leave event.
				// Flip the mouse_in bit.
				m_bitfield ^= static_cast<unsigned char>( 0x10 );

				GetSignals().Emit( OnMouseLeave );
				HandleMouseLeave( event.mouseMove.x, event.mouseMove.y );
			}

			HandleMouseMoveEvent( event.mouseMove.x, event.mouseMove.y );
			break;

		case sf::Event::MouseButtonPressed:
			if( ( ( m_bitfield & static_cast<unsigned char>( 0xe0 ) ) == static_cast<unsigned char>( 0xe0 ) ) && IsMouseInWidget() ) {
				// Clear the mouse_button_down bits to 0s.
				m_bitfield &= static_cast<unsigned char>( 0x1f );

				// Set the mouse_button_down bits.
				m_bitfield |= static_cast<unsigned char>( event.mouseButton.button << 5 );
			}

			HandleMouseButtonEvent( event.mouseButton.button, true, event.mouseButton.x, event.mouseButton.y );

			if( IsMouseInWidget() ) {
				if( event.mouseButton.button == sf::Mouse::Left ) {
					GetSignals().Emit( OnMouseLeftPress );
				}
				else if( event.mouseButton.button == sf::Mouse::Right ) {
					GetSignals().Emit( OnMouseRightPress );
				}
			}

			break;

		case sf::Event::MouseButtonReleased:
			// Only process as a click when mouse button has been pressed inside the widget before.
			if( ( ( m_bitfield & 0xe0 ) >> 5 ) == event.mouseButton.button ) {
				// Set the mouse_button_down bits to 111 (none).
				m_bitfield |= static_cast<unsigned char>( 0xe0 );

				// When released inside the widget, the event can be considered a click.
				if( IsMouseInWidget() ) {
					HandleMouseClick( event.mouseButton.button, event.mouseButton.x, event.mouseButton.y );

					if( event.mouseButton.button == sf::Mouse::Left ) {
						GetSignals().Emit( OnLeftClick );
					}
					else if( event.mouseButton.button == sf::Mouse::Right ) {
						GetSignals().Emit( OnRightClick );
					}
				}
			}

			HandleMouseButtonEvent( event.mouseButton.button, false, event.mouseButton.x, event.mouseButton.y );

			if( IsMouseInWidget() ) {
				if( event.mouseButton.button == sf::Mouse::Left ) {
					GetSignals().Emit( OnMouseLeftRelease );
				}
				else if( event.mouseButton.button == sf::Mouse::Right ) {
					GetSignals().Emit( OnMouseRightRelease );
				}
			}

			break;

		case sf::Event::KeyPressed:
			if( HasFocus() ) {
				// TODO: Delegate event too when widget's not active?
				HandleKeyEvent( event.key.code, true );
				GetSignals().Emit( OnKeyPress );
			}

			break;

		case sf::Event::KeyReleased:
			if( HasFocus() ) {
				// TODO: Delegate event too when widget's not active?
				HandleKeyEvent( event.key.code, false );
				GetSignals().Emit( OnKeyRelease );
			}
			break;

		case sf::Event::TextEntered:
			if( HasFocus() ) {
				// TODO: Delegate event too when widget's not active?
				HandleTextEvent( event.text.unicode );
				GetSignals().Emit( OnText );
			}
			break;

		default:
			break;
	}
}

void Widget::SetState( State state ) {
	// Do nothing if state wouldn't change.
	if( GetState() == state ) {
		return;
	}

	unsigned char old_state( GetState() );

	// Clear the state bits to 0s.
	m_bitfield &= static_cast<unsigned char>( 0xf1 );

	// Store the new state.
	m_bitfield |= static_cast<unsigned char>( state << 1 );

	// If HandleStateChange() changed the state, do not call observer, will be
	// done from there too.
	if( GetState() != old_state ) {
		HandleStateChange( static_cast<State>( old_state ) );
		GetSignals().Emit( OnStateChange );
	}

	if( state == ACTIVE ) {
		GrabFocus( shared_from_this() );
		SetActiveWidget( shared_from_this() );
	}
	else if( old_state == ACTIVE ) {
		SetActiveWidget( Ptr() );
	}
}

Widget::State Widget::GetState() const {
	return static_cast<State>( ( ( m_bitfield & 0x0e ) >> 1 ) );
}

Container::Ptr Widget::GetParent() {
	return m_parent.lock();
}

Container::PtrConst Widget::GetParent() const {
	return m_parent.lock();
}

void Widget::GrabFocus() {
	GrabFocus( shared_from_this() );
}

bool Widget::HasFocus() const {
	return HasFocus( shared_from_this() );
}

bool Widget::IsMouseInWidget() const {
	return ( m_bitfield & static_cast<unsigned char>( 0x10 ) ) != 0;
}

void Widget::Show( bool show ) {
	if( show == IsLocallyVisible() ) {
		return;
	}

	bool old_global_visibility = IsGloballyVisible();

	// Flip the visible bit since we know show != IsLocallyVisible()
	m_bitfield ^= 0x01;

	HandleLocalVisibilityChange();

	if( old_global_visibility != IsGloballyVisible() ) {
		HandleGlobalVisibilityChange();
	}

	RequestResize();
}

const sf::Vector2f& Widget::GetRequisition() const {
	return m_requisition;
}

void Widget::SetRequisition( const sf::Vector2f& requisition ) {
	if( requisition.x > 0.f || requisition.y > 0.f ) {
		delete m_custom_requisition;
		m_custom_requisition = new sf::Vector2f( requisition );
	}
	else {
		delete m_custom_requisition;
		m_custom_requisition = 0;
	}

	RequestResize();
}

sf::Vector2f Widget::GetAbsolutePosition() const {
	// If no parent, allocation's position is absolute position.
	PtrConst parent( m_parent.lock() );

	if( !parent ) {
		return sf::Vector2f( GetAllocation().left, GetAllocation().top );
	}

	// Get parent's absolute position and add own rel. position to it.
	sf::Vector2f parent_position( parent->GetAbsolutePosition() );

	return sf::Vector2f(
		parent_position.x + GetAllocation().left,
		parent_position.y + GetAllocation().top
	);
}

void Widget::UpdateDrawablePosition() const {
	if( m_drawable ) {
		m_drawable->SetPosition( GetAbsolutePosition() );
	}
}

void Widget::SetId( const std::string& id ) {
	if( id.empty() ) {
		return;
	}

	if( !m_class_id ) {
		m_class_id = new struct ClassId;
	}

	m_class_id->id = id;
}

std::string Widget::GetId() const {
	if( !m_class_id ) {
		return "";
	}

	return m_class_id->id;
}

void Widget::SetClass( const std::string& cls ) {
	if( cls.empty() ) {
		return;
	}

	if( !m_class_id ) {
		m_class_id = new struct ClassId;
	}

	m_class_id->class_ = cls;
}

std::string Widget::GetClass() const {
	if( !m_class_id ) {
		return "";
	}

	return m_class_id->class_;
}

void Widget::HandleMouseMoveEvent( int /*x*/, int /*y*/ ) {
}

void Widget::HandleMouseButtonEvent( sf::Mouse::Button /*button*/, bool /*press*/, int /*x*/, int /*y*/ ) {
}

void Widget::HandleKeyEvent( sf::Keyboard::Key /*key*/, bool /*press*/ ) {
}

void Widget::HandlePositionChange() {
}

void Widget::HandleSizeChange() {
}

void Widget::HandleStateChange( State /*old_state*/ ) {
	Invalidate();
}

void Widget::HandleTextEvent( sf::Uint32 /*character*/ ) {
}

void Widget::HandleMouseEnter( int /*x*/, int /*y*/ ) {
}

void Widget::HandleMouseLeave( int /*x*/, int /*y*/ ) {
}

void Widget::HandleMouseClick( sf::Mouse::Button /*button*/, int /*x*/, int /*y*/ ) {
}

void Widget::HandleFocusChange( const Widget::Ptr& focused_widget ) {
	if( ( focused_widget != shared_from_this() ) && ( GetState() == ACTIVE ) ) {
		SetState( NORMAL );
	}
}

void Widget::HandleLocalVisibilityChange() {
}

void Widget::HandleGlobalVisibilityChange() {
	if( GetState() == PRELIGHT ) {
		SetState( NORMAL );
	}

	if( m_drawable ) {
		m_drawable->Show( IsGloballyVisible() );
	}
}

void Widget::HandleAbsolutePositionChange() {
	UpdateDrawablePosition();
}

void Widget::Refresh() {
	RequestResize();

	Invalidate();
}

void Widget::HandleRequisitionChange() {
}

void Widget::HandleUpdate( float /*seconds*/ ) {
}

void Widget::HandleSetHierarchyLevel() {
	if( m_drawable ) {
		m_drawable->SetLevel( m_hierarchy_level );
	}
}

void Widget::SetHierarchyLevel( int level ) {
	m_hierarchy_level = level;

	HandleSetHierarchyLevel();
}

int Widget::GetHierarchyLevel() const {
	return m_hierarchy_level;
}

void Widget::SetViewport( const RendererViewport::Ptr& viewport ) {
	m_viewport = viewport;

	HandleViewportUpdate();
}

const RendererViewport::Ptr& Widget::GetViewport() const {
	return m_viewport;
}

int Widget::GetZOrder() const {
	return m_z_order;
}

void Widget::SetZOrder( int z_order ) {
	m_z_order = z_order;

	if( m_drawable ) {
		m_drawable->SetZOrder( z_order );
	}
}

void Widget::HandleViewportUpdate() {
	if( m_drawable ) {
		m_drawable->SetViewport( m_viewport );
	}
}

void Widget::SetActiveWidget() {
	SetActiveWidget( shared_from_this() );
}

void Widget::SetActiveWidget( Ptr widget ) {
	m_active_widget = widget;
}

bool Widget::IsActiveWidget() const {
	return IsActiveWidget( shared_from_this() );
}

bool Widget::IsActiveWidget( PtrConst widget ) {
	if( m_active_widget.lock() == widget ) {
		return true;
	}

	return false;
}

void Widget::GrabModal() {
	if( m_modal_widget.lock() ) {
#ifdef SFGUI_DEBUG
		std::cerr << "SFGUI warning: Tried to grab modal while existing widget has it.\n";
#endif

		return;
	}

	m_modal_widget = shared_from_this();
}

void Widget::ReleaseModal() {
	if( m_modal_widget.lock() == shared_from_this() ) {
		m_modal_widget.reset();

		return;
	}

#ifdef SFGUI_DEBUG
	std::cerr << "SFGUI warning: Tried to release modal although current widget not modal.\n";
#endif
}

bool Widget::IsModal() const {
	if( m_modal_widget.lock() == shared_from_this() ) {
		return true;
	}

	return false;
}

bool Widget::HasModal() {
	return m_modal_widget.lock();
}

}
