Strict
Import vsat


Class VScene Abstract
	Field shouldClearScreen:Bool = True
	
	Method OnUpdate:Void(delta:Float) End
	Method OnRender:Void() End
	Method OnLoading:Void() End
	Method OnInit:Void() End
	Method OnExit:Void() End
	Method OnSuspend:Void() End
	Method OnResume:Void() End
	Method OnResize:Void() End
	Method OnUpdateWhilePaused:Void() End
	Method HandleEvent:Void(event:VEvent) End
		
	Method ClearScreen:Void()
		ClearScreenWithColor(Color.Gray)
	End
	
End
