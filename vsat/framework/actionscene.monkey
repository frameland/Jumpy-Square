Strict
Import app
Import scene
Import actions

Class VActionScene Extends VScene Implements VActionEventHandler
	
	Method AddAction:Void(action:VAction)
		action.AddToList(actions)
		action.SetListener(Self)
	End
	
	Method UpdateActions:Void(dt:Float)
		VAction.UpdateList(actions, dt)
	End
	
	Method OnActionEvent:Void(id:Int, action:VAction)
	End
	
	Private
	Field actions:List<VAction> = New List<VAction>
	
End
