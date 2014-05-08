Strict
Import vsat.foundation
#If TARGET = "ios"
	Import brl.gamecenter
#End

Const GAMECENTER_LEADERBOARD:String = "highscore"


Function InitGameCenter:Void()
	#If TARGET = "ios"
		gameCenter = GameCenter.GetGameCenter()
		If gameCenter.GameCenterState() = 0
			gameCenter.StartGameCenter()
		End
	#End
End

Function SyncGameCenter:Void(withScore:Int)
	#If TARGET = "ios"
		If not gameCenter Return
		If gameCenter.GameCenterState() = 2 And gameCenter.GameCenterAvail()
			gameCenter.ReportScore(withScore, GAMECENTER_LEADERBOARD)
		End
	#End
End

Function ShowGameCenter:Void()
	#If TARGET = "ios"
		If not gameCenter Return
		If gameCenter.GameCenterAvail() And gameCenter.GameCenterState() = 2
			gameCenter.ShowLeaderboard(GAMECENTER_LEADERBOARD)
		End
	#End
End

Function GameCenterIsConnecting:Bool()
	#If TARGET = "ios"
		If not gameCenter Return False
		Return gameCenter.GameCenterState() = 1
	#End
	Return False
End


Private
Global gameCenter:GameCenter
