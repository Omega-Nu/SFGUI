#include <SFGUI/Bin.hpp>

namespace sfg {

Bin::Bin() :
	Container()
{
}

Widget::Ptr Bin::GetChild() const {
	if( GetChildren().size() < 1 ) {
		return Widget::Ptr();
	}

	return *GetChildren().begin();
}

}