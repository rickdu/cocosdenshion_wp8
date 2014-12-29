
namespace CocosDenshion
{
	public enum class MusicEvent
	{
		Menu,
		Gameplay,
		FrontEnd,
		Level1,
		Level2,

		LastMusicEvent,
		None = LastMusicEvent
	};

	public ref class MusicEventEnum sealed
	{
	public:
		property MusicEvent MusicEventId
		{
			MusicEvent  get()
			{
				return m_event;
			}
		}

	private:
		MusicEvent m_event;
	};


}